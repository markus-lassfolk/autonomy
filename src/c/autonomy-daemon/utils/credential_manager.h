#ifndef CREDENTIAL_MANAGER_H
#define CREDENTIAL_MANAGER_H

#include <stdbool.h>
#include <stddef.h>

// Maximum lengths for credentials
#define CRED_MAX_KEY_LENGTH 256
#define CRED_MAX_VALUE_LENGTH 512
#define CRED_MAX_SERVICE_NAME 64

// Credential types
typedef enum {
    CRED_TYPE_API_KEY,
    CRED_TYPE_TOKEN,
    CRED_TYPE_PASSWORD,
    CRED_TYPE_CERTIFICATE
} credential_type_t;

// Credential storage methods
typedef enum {
    CRED_STORAGE_UCI,          // OpenWrt UCI configuration
    CRED_STORAGE_FILE,         // Encrypted file storage
    CRED_STORAGE_ENV,          // Environment variables
    CRED_STORAGE_MEMORY        // In-memory only (for testing)
} credential_storage_t;

// Credential structure
typedef struct {
    char service_name[CRED_MAX_SERVICE_NAME];
    char key[CRED_MAX_KEY_LENGTH];
    char value[CRED_MAX_VALUE_LENGTH];
    credential_type_t type;
    credential_storage_t storage_method;
    bool encrypted;
    time_t created;
    time_t last_accessed;
} credential_t;

// Credential manager configuration
typedef struct {
    credential_storage_t default_storage;
    bool enable_encryption;
    bool enable_access_logging;
    char encryption_key[32];  // 256-bit key
} credential_manager_config_t;

// Initialize credential manager
int credential_manager_init(const credential_manager_config_t* config);

// Cleanup credential manager
void credential_manager_cleanup(void);

// Store credential
int credential_store(const char* service_name, const char* key, const char* value, 
                    credential_type_t type, credential_storage_t storage);

// Retrieve credential
int credential_get(const char* service_name, const char* key, char* value, size_t value_size);

// Check if credential exists
bool credential_exists(const char* service_name, const char* key);

// Delete credential
int credential_delete(const char* service_name, const char* key);

// List all credentials for a service
int credential_list_service(const char* service_name, char** keys, size_t max_keys);

// Update credential
int credential_update(const char* service_name, const char* key, const char* new_value);

// Rotate credential (generate new value)
int credential_rotate(const char* service_name, const char* key, char* new_value, size_t value_size);

// Validate credential format
bool credential_validate(const char* value, credential_type_t type);

// Get credential metadata
int credential_get_metadata(const char* service_name, const char* key, 
                           time_t* created, time_t* last_accessed);

// Encrypt/decrypt utilities
int credential_encrypt(const char* plaintext, char* ciphertext, size_t cipher_size);
int credential_decrypt(const char* ciphertext, char* plaintext, size_t plain_size);

// UCI integration
int credential_load_from_uci(const char* service_name);
int credential_save_to_uci(const char* service_name);

// Common service names
#define CRED_SERVICE_OPENWEATHER "openweather"
#define CRED_SERVICE_GOOGLE_MAPS "google_maps"
#define CRED_SERVICE_SPACE_TRACK "space_track"
#define CRED_SERVICE_STARLINK "starlink"
#define CRED_SERVICE_MQTT "mqtt"
#define CRED_SERVICE_CELLULAR "cellular"

// Common credential keys
#define CRED_KEY_API_KEY "api_key"
#define CRED_KEY_SECRET "secret"
#define CRED_KEY_TOKEN "token"
#define CRED_KEY_USERNAME "username"
#define CRED_KEY_PASSWORD "password"
#define CRED_KEY_CLIENT_ID "client_id"
#define CRED_KEY_CLIENT_SECRET "client_secret"

#endif // CREDENTIAL_MANAGER_H
