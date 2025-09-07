#include "credential_manager.h"
#include "logx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/aes.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

// Global credential manager state
static struct {
    credential_manager_config_t config;
    bool initialized;
    credential_t* credentials;
    size_t credential_count;
    size_t credential_capacity;
    pthread_mutex_t mutex;
} g_credential_manager = {0};

// Internal functions
static int expand_credential_storage(void);
static int find_credential(const char* service_name, const char* key);
static int load_credentials_from_storage(void);
static int save_credentials_to_storage(void);
static int generate_encryption_key(void);

// Initialize credential manager
int credential_manager_init(const credential_manager_config_t* config) {
    if (g_credential_manager.initialized) {
        return 0; // Already initialized
    }

    if (!config) {
        LOGX_ERROR_MSG("Invalid credential manager configuration");
        return -1;
    }

    memcpy(&g_credential_manager.config, config, sizeof(credential_manager_config_t));
    
    // Initialize mutex
    if (pthread_mutex_init(&g_credential_manager.mutex, NULL) != 0) {
        LOGX_ERROR_MSG("Failed to initialize credential manager mutex");
        return -1;
    }

    // Initialize storage
    g_credential_manager.credential_capacity = 64;
    g_credential_manager.credentials = calloc(g_credential_manager.credential_capacity, 
                                            sizeof(credential_t));
    if (!g_credential_manager.credentials) {
        LOGX_ERROR_MSG("Failed to allocate credential storage");
        pthread_mutex_destroy(&g_credential_manager.mutex);
        return -1;
    }

    // Generate encryption key if needed
    if (g_credential_manager.config.enable_encryption) {
        if (generate_encryption_key() != 0) {
            LOGX_WARN_MSG("Failed to generate encryption key, disabling encryption");
            g_credential_manager.config.enable_encryption = false;
        }
    }

    // Load existing credentials
    load_credentials_from_storage();

    g_credential_manager.initialized = true;
    
    LOGX_INFO_MSG("Credential manager initialized", 
                  "storage", g_credential_manager.config.default_storage == CRED_STORAGE_UCI ? "UCI" : "FILE",
                  "encryption", g_credential_manager.config.enable_encryption ? "enabled" : "disabled");

    return 0;
}

// Cleanup credential manager
void credential_manager_cleanup(void) {
    if (!g_credential_manager.initialized) {
        return;
    }

    pthread_mutex_lock(&g_credential_manager.mutex);

    // Save credentials
    save_credentials_to_storage();

    // Clear sensitive data
    if (g_credential_manager.credentials) {
        for (size_t i = 0; i < g_credential_manager.credential_count; i++) {
            memset(&g_credential_manager.credentials[i], 0, sizeof(credential_t));
        }
        free(g_credential_manager.credentials);
        g_credential_manager.credentials = NULL;
    }

    // Clear encryption key
    memset(g_credential_manager.config.encryption_key, 0, 
           sizeof(g_credential_manager.config.encryption_key));

    g_credential_manager.initialized = false;
    
    pthread_mutex_unlock(&g_credential_manager.mutex);
    pthread_mutex_destroy(&g_credential_manager.mutex);

    LOGX_INFO_MSG("Credential manager cleaned up");
}

// Store credential
int credential_store(const char* service_name, const char* key, const char* value, 
                    credential_type_t type, credential_storage_t storage) {
    if (!g_credential_manager.initialized || !service_name || !key || !value) {
        return -1;
    }

    pthread_mutex_lock(&g_credential_manager.mutex);

    // Check if credential already exists
    int existing_index = find_credential(service_name, key);
    
    credential_t* cred = NULL;
    if (existing_index >= 0) {
        // Update existing credential
        cred = &g_credential_manager.credentials[existing_index];
    } else {
        // Add new credential
        if (g_credential_manager.credential_count >= g_credential_manager.credential_capacity) {
            if (expand_credential_storage() != 0) {
                pthread_mutex_unlock(&g_credential_manager.mutex);
                return -1;
            }
        }
        cred = &g_credential_manager.credentials[g_credential_manager.credential_count++];
        cred->created = time(NULL);
    }

    // Populate credential
    strncpy(cred->service_name, service_name, sizeof(cred->service_name) - 1);
    strncpy(cred->key, key, sizeof(cred->key) - 1);
    
    // Encrypt value if encryption is enabled
    if (g_credential_manager.config.enable_encryption) {
        if (credential_encrypt(value, cred->value, sizeof(cred->value)) != 0) {
            LOGX_WARN_MSG("Failed to encrypt credential, storing in plaintext");
            strncpy(cred->value, value, sizeof(cred->value) - 1);
            cred->encrypted = false;
        } else {
            cred->encrypted = true;
        }
    } else {
        strncpy(cred->value, value, sizeof(cred->value) - 1);
        cred->encrypted = false;
    }

    cred->type = type;
    cred->storage_method = storage;
    cred->last_accessed = time(NULL);

    // Save to persistent storage based on storage method
    if (storage == CRED_STORAGE_UCI) {
        credential_save_to_uci(service_name);
    } else if (storage == CRED_STORAGE_FILE) {
        save_credentials_to_storage();
    }

    pthread_mutex_unlock(&g_credential_manager.mutex);

    LOGX_INFO_MSG("Credential stored", "service", service_name, "key", key);
    return 0;
}

// Retrieve credential
int credential_get(const char* service_name, const char* key, char* value, size_t value_size) {
    if (!g_credential_manager.initialized || !service_name || !key || !value || value_size == 0) {
        return -1;
    }

    pthread_mutex_lock(&g_credential_manager.mutex);

    int index = find_credential(service_name, key);
    if (index < 0) {
        pthread_mutex_unlock(&g_credential_manager.mutex);
        return -1;
    }

    credential_t* cred = &g_credential_manager.credentials[index];
    
    // Update last accessed time
    cred->last_accessed = time(NULL);

    // Decrypt if needed
    if (cred->encrypted) {
        if (credential_decrypt(cred->value, value, value_size) != 0) {
            LOGX_ERROR_MSG("Failed to decrypt credential");
            pthread_mutex_unlock(&g_credential_manager.mutex);
            return -1;
        }
    } else {
        strncpy(value, cred->value, value_size - 1);
        value[value_size - 1] = '\0';
    }

    pthread_mutex_unlock(&g_credential_manager.mutex);

    if (g_credential_manager.config.enable_access_logging) {
        LOGX_DEBUG_MSG("Credential accessed", "service", service_name, "key", key);
    }

    return 0;
}

// Check if credential exists
bool credential_exists(const char* service_name, const char* key) {
    if (!g_credential_manager.initialized || !service_name || !key) {
        return false;
    }

    pthread_mutex_lock(&g_credential_manager.mutex);
    bool exists = find_credential(service_name, key) >= 0;
    pthread_mutex_unlock(&g_credential_manager.mutex);

    return exists;
}

// Delete credential
int credential_delete(const char* service_name, const char* key) {
    if (!g_credential_manager.initialized || !service_name || !key) {
        return -1;
    }

    pthread_mutex_lock(&g_credential_manager.mutex);

    int index = find_credential(service_name, key);
    if (index < 0) {
        pthread_mutex_unlock(&g_credential_manager.mutex);
        return -1;
    }

    // Clear sensitive data
    memset(&g_credential_manager.credentials[index], 0, sizeof(credential_t));

    // Shift remaining credentials
    for (size_t i = index; i < g_credential_manager.credential_count - 1; i++) {
        memcpy(&g_credential_manager.credentials[i], 
               &g_credential_manager.credentials[i + 1], 
               sizeof(credential_t));
    }
    
    g_credential_manager.credential_count--;

    save_credentials_to_storage();

    pthread_mutex_unlock(&g_credential_manager.mutex);

    LOGX_INFO_MSG("Credential deleted", "service", service_name, "key", key);
    return 0;
}

// Encrypt credential value
int credential_encrypt(const char* plaintext, char* ciphertext, size_t cipher_size) {
    if (!plaintext || !ciphertext || cipher_size == 0 || 
        !g_credential_manager.config.enable_encryption) {
        return -1;
    }

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return -1;

    unsigned char iv[16];
    if (RAND_bytes(iv, sizeof(iv)) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, 
                          (unsigned char*)g_credential_manager.config.encryption_key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    int len;
    unsigned char* encrypted = malloc(strlen(plaintext) + AES_BLOCK_SIZE);
    if (!encrypted) {
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    if (EVP_EncryptUpdate(ctx, encrypted, &len, (unsigned char*)plaintext, strlen(plaintext)) != 1) {
        free(encrypted);
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    int ciphertext_len = len;
    if (EVP_EncryptFinal_ex(ctx, encrypted + len, &len) != 1) {
        free(encrypted);
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    ciphertext_len += len;

    // Encode as base64 with IV prepended
    size_t total_len = sizeof(iv) + ciphertext_len;
    unsigned char* combined = malloc(total_len);
    if (!combined) {
        free(encrypted);
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    memcpy(combined, iv, sizeof(iv));
    memcpy(combined + sizeof(iv), encrypted, ciphertext_len);

    // Simple base64 encoding (in production, use proper base64 library)
    size_t encoded_len = ((total_len + 2) / 3) * 4;
    if (encoded_len >= cipher_size) {
        free(encrypted);
        free(combined);
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }

    // Use proper base64 encoding for encrypted data
    int base64_len = ((total_len + 2) / 3) * 4 + 1; // Base64 encoding size
    if (base64_len >= cipher_size) {
        free(encrypted);
        free(combined);
        EVP_CIPHER_CTX_free(ctx);
        return -1;
    }
    
    // Encode to base64
    BIO *bio, *b64;
    BUF_MEM *bufferPtr;
    
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);
    
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); // No newlines in output
    BIO_write(bio, combined, total_len);
    BIO_flush(bio);
    
    BIO_get_mem_ptr(bio, &bufferPtr);
    memcpy(ciphertext, bufferPtr->data, bufferPtr->length);
    ciphertext[bufferPtr->length] = '\0';
    
    BIO_free_all(bio);

    free(encrypted);
    free(combined);
    EVP_CIPHER_CTX_free(ctx);

    return 0;
}

// Decrypt credential value
int credential_decrypt(const char* ciphertext, char* plaintext, size_t plain_size) {
    if (!ciphertext || !plaintext || plain_size == 0) {
        return -1;
    }

    // Convert hex back to binary
    size_t cipher_len = strlen(ciphertext) / 2;
    unsigned char* combined = malloc(cipher_len);
    if (!combined) return -1;

    for (size_t i = 0; i < cipher_len; i++) {
        sscanf(ciphertext + i * 2, "%2hhx", &combined[i]);
    }

    if (cipher_len < 16) {
        free(combined);
        return -1;
    }

    unsigned char* iv = combined;
    unsigned char* encrypted = combined + 16;
    size_t encrypted_len = cipher_len - 16;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) {
        free(combined);
        return -1;
    }

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL,
                          (unsigned char*)g_credential_manager.config.encryption_key, iv) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        free(combined);
        return -1;
    }

    int len;
    unsigned char* decrypted = malloc(encrypted_len + AES_BLOCK_SIZE);
    if (!decrypted) {
        EVP_CIPHER_CTX_free(ctx);
        free(combined);
        return -1;
    }

    if (EVP_DecryptUpdate(ctx, decrypted, &len, encrypted, encrypted_len) != 1) {
        free(decrypted);
        EVP_CIPHER_CTX_free(ctx);
        free(combined);
        return -1;
    }

    int plaintext_len = len;
    if (EVP_DecryptFinal_ex(ctx, decrypted + len, &len) != 1) {
        free(decrypted);
        EVP_CIPHER_CTX_free(ctx);
        free(combined);
        return -1;
    }
    plaintext_len += len;

    if ((size_t)plaintext_len >= plain_size) {
        free(decrypted);
        EVP_CIPHER_CTX_free(ctx);
        free(combined);
        return -1;
    }

    memcpy(plaintext, decrypted, plaintext_len);
    plaintext[plaintext_len] = '\0';

    free(decrypted);
    EVP_CIPHER_CTX_free(ctx);
    free(combined);

    return 0;
}

// UCI integration
int credential_save_to_uci(const char* service_name) {
    char cmd[512];
    pthread_mutex_lock(&g_credential_manager.mutex);

    for (size_t i = 0; i < g_credential_manager.credential_count; i++) {
        credential_t* cred = &g_credential_manager.credentials[i];
        if (strcmp(cred->service_name, service_name) == 0) {
            snprintf(cmd, sizeof(cmd), 
                    "uci set autonomy.credentials.%s_%s='%s' && uci commit autonomy",
                    service_name, cred->key, cred->value);
            system(cmd);
        }
    }

    pthread_mutex_unlock(&g_credential_manager.mutex);
    return 0;
}

// Load credentials from UCI
int credential_load_from_uci(const char* service_name) {
    char cmd[256];
    FILE* fp;
    char line[512];

    snprintf(cmd, sizeof(cmd), "uci show autonomy.credentials 2>/dev/null | grep '^autonomy.credentials.%s_'", 
             service_name);
    
    fp = popen(cmd, "r");
    if (!fp) return -1;

    while (fgets(line, sizeof(line), fp)) {
        char* key_start = strstr(line, service_name);
        if (!key_start) continue;

        key_start += strlen(service_name) + 1; // Skip service name and underscore
        char* value_start = strchr(key_start, '=');
        if (!value_start) continue;

        *value_start = '\0';
        value_start++;

        // Remove quotes and newline
        if (*value_start == '\'') value_start++;
        char* value_end = strchr(value_start, '\'');
        if (value_end) *value_end = '\0';
        else {
            value_end = strchr(value_start, '\n');
            if (value_end) *value_end = '\0';
        }

        credential_store(service_name, key_start, value_start, CRED_TYPE_API_KEY, CRED_STORAGE_UCI);
    }

    pclose(fp);
    return 0;
}

// Internal helper functions
static int find_credential(const char* service_name, const char* key) {
    for (size_t i = 0; i < g_credential_manager.credential_count; i++) {
        if (strcmp(g_credential_manager.credentials[i].service_name, service_name) == 0 &&
            strcmp(g_credential_manager.credentials[i].key, key) == 0) {
            return i;
        }
    }
    return -1;
}

static int expand_credential_storage(void) {
    size_t new_capacity = g_credential_manager.credential_capacity * 2;
    credential_t* new_credentials = realloc(g_credential_manager.credentials, 
                                           new_capacity * sizeof(credential_t));
    if (!new_credentials) {
        return -1;
    }

    g_credential_manager.credentials = new_credentials;
    g_credential_manager.credential_capacity = new_capacity;
    return 0;
}

static int load_credentials_from_storage(void) {
    // Load from default storage method
    if (g_credential_manager.config.default_storage == CRED_STORAGE_UCI) {
        // Load common services from UCI
        credential_load_from_uci(CRED_SERVICE_OPENWEATHER);
        credential_load_from_uci(CRED_SERVICE_GOOGLE_MAPS);
        credential_load_from_uci(CRED_SERVICE_SPACE_TRACK);
        credential_load_from_uci(CRED_SERVICE_STARLINK);
        credential_load_from_uci(CRED_SERVICE_MQTT);
    }
    return 0;
}

static int save_credentials_to_storage(void) {
    // Implementation depends on storage method
    return 0;
}

static int generate_encryption_key(void) {
    return RAND_bytes((unsigned char*)g_credential_manager.config.encryption_key, 32) == 1 ? 0 : -1;
}
