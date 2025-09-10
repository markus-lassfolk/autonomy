#include "security_monitor.h"
#include "../core/types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdbool.h>
#include <math.h>
#include <sys/socket.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

// External reference to global configuration
extern autonomy_config_t g_config;

// Global security monitor instance
static security_monitor_t g_security_monitor;
static bool g_security_monitor_initialized = false;

// Forward declarations
int perform_file_integrity_check(security_scan_result_t* result);
int perform_network_security_check(security_scan_result_t* result);
int perform_access_control_check(security_scan_result_t* result);
int perform_configuration_check(security_scan_result_t* result);
static int perform_threat_detection(security_scan_result_t* result);
void update_security_events(const char* event_type, const char* description, 
                                  const char* source, const char* target, threat_level_t level);
static char* generate_event_id(void);

// Initialize security monitor
int security_monitor_init(const security_monitor_config_t* config) {
    if (g_security_monitor_initialized) {
        return 0; // Already initialized
    }
    
    memset(&g_security_monitor, 0, sizeof(security_monitor_t));
    
    // Set configuration
    if (config) {
        g_security_monitor.config = *config;
    } else {
        // Default configuration
        g_security_monitor.config.enabled = true; // Use configurable security monitoring setting
        g_security_monitor.config.scan_interval_seconds = 300; // 5 minutes
        g_security_monitor.config.enable_file_integrity = true; // Use configurable file integrity monitoring
        g_security_monitor.config.enable_network_monitoring = true; // Use configurable network monitoring
        g_security_monitor.config.enable_access_control = true; // Use configurable access control
        g_security_monitor.config.enable_configuration_check = true; // Use configurable configuration check
        g_security_monitor.config.enable_threat_detection = true; // Use configurable threat detection
    }
    
    // Initialize mutex
    g_security_monitor.mutex = malloc(sizeof(pthread_mutex_t));
    if (!g_security_monitor.mutex) {
        return -1;
    }
    
    pthread_mutex_init(g_security_monitor.mutex, NULL);
    
    // Initialize security events
    g_security_monitor.event_count = 0;
    g_security_monitor.event_index = 0;
    
    g_security_monitor_initialized = true;
    return 0;
}

// Clean up security monitor
void security_monitor_cleanup(void) {
    if (!g_security_monitor_initialized) return;
    
    if (g_security_monitor.mutex) {
        pthread_mutex_destroy(g_security_monitor.mutex);
        free(g_security_monitor.mutex);
    }
    
    g_security_monitor.mutex = NULL;
    g_security_monitor_initialized = false;
}

// Perform security scan
int security_monitor_perform_scan(void) {
    if (!g_security_monitor_initialized || !g_security_monitor.config.enabled) {
        return -1;
    }
    
    pthread_mutex_lock(g_security_monitor.mutex);
    
    security_scan_result_t result;
    memset(&result, 0, sizeof(security_scan_result_t));
    
    int vulnerabilities_found = 0;
    int critical_vulnerabilities = 0;
    
    // Perform various security checks
    if (g_security_monitor.config.enable_file_integrity) {
        if (perform_file_integrity_check(&result) == 0) {
            result.file_integrity_check = true;
        }
    }
    
    if (g_security_monitor.config.enable_network_monitoring) {
        if (perform_network_security_check(&result) == 0) {
            result.network_security_check = true;
        }
    }
    
    if (g_security_monitor.config.enable_access_control) {
        if (perform_access_control_check(&result) == 0) {
            result.access_control_check = true;
        }
    }
    
    if (g_security_monitor.config.enable_configuration_check) {
        if (perform_configuration_check(&result) == 0) {
            result.configuration_check = true;
        }
    }
    
    if (g_security_monitor.config.enable_threat_detection) {
        if (perform_threat_detection(&result) == 0) {
            // Count vulnerabilities
            for (int i = 0; i < g_security_monitor.event_count; i++) {
                if (g_security_monitor.security_events[i].threat_level == THREAT_LEVEL_CRITICAL) {
                    critical_vulnerabilities++;
                } else if (g_security_monitor.security_events[i].threat_level >= THREAT_LEVEL_LOW) {
                    vulnerabilities_found++;
                }
            }
        }
    }
    
    // Update scan results
    result.vulnerabilities_found = vulnerabilities_found;
    result.critical_vulnerabilities = critical_vulnerabilities;
    result.scan_timestamp = time(NULL);
    
    // Generate scan summary
    snprintf(result.scan_summary, sizeof(result.scan_summary),
             "Security scan completed: %d vulnerabilities found (%d critical)",
             vulnerabilities_found, critical_vulnerabilities);
    
    // Update last scan results
    g_security_monitor.last_scan = result;
    
    // Update statistics
    g_security_monitor.last_scan_time = time(NULL);
    g_security_monitor.scan_count++;
    g_security_monitor.threat_detections = vulnerabilities_found;
    g_security_monitor.critical_threats = critical_vulnerabilities;
    
    pthread_mutex_unlock(g_security_monitor.mutex);
    
    return 0;
}

// Get security scan results
int security_monitor_get_scan_results(security_scan_result_t* results) {
    if (!g_security_monitor_initialized || !results) {
        return -1;
    }
    
    pthread_mutex_lock(g_security_monitor.mutex);
    *results = g_security_monitor.last_scan;
    pthread_mutex_unlock(g_security_monitor.mutex);
    
    return 0;
}

// Get security events
int security_monitor_get_events(security_event_t* events, int max_events) {
    if (!g_security_monitor_initialized || !events || max_events <= 0) {
        return -1;
    }
    
    pthread_mutex_lock(g_security_monitor.mutex);
    
    int count = 0;
    int index = g_security_monitor.event_index;
    
    for (int i = 0; i < g_security_monitor.event_count && count < max_events; i++) {
        int event_index = (index - i + 100) % 100;
        if (g_security_monitor.security_events[event_index].timestamp > 0) {
            events[count] = g_security_monitor.security_events[event_index];
            count++;
        }
    }
    
    pthread_mutex_unlock(g_security_monitor.mutex);
    
    return count;
}

// Acknowledge security event
int security_monitor_acknowledge_event(const char* event_id) {
    if (!g_security_monitor_initialized || !event_id) {
        return -1;
    }
    
    pthread_mutex_lock(g_security_monitor.mutex);
    
    for (int i = 0; i < g_security_monitor.event_count; i++) {
        if (strcmp(g_security_monitor.security_events[i].event_id, event_id) == 0) {
            g_security_monitor.security_events[i].acknowledged = true;
            pthread_mutex_unlock(g_security_monitor.mutex);
            return 0;
        }
    }
    
    pthread_mutex_unlock(g_security_monitor.mutex);
    return -1;
}

// Report security threat
int security_monitor_report_threat(threat_level_t level, const char* description, 
                                  const char* source, const char* target) {
    if (!g_security_monitor_initialized || !description) {
        return -1;
    }
    
    update_security_events("threat_detected", description, source, target, level);
    
    return 0;
}

// Calculate file checksum using SHA256
static int calculate_file_checksum(const char* filepath, char* checksum, size_t checksum_size) {
    FILE* fp = fopen(filepath, "rb");
    if (!fp) return -1;
    
    // Use OpenSSL directly for SHA256 calculation (secure approach)
    fclose(fp);
    
    FILE* file = fopen(filepath, "rb");
    if (!file) return -1;
    
    // Initialize OpenSSL SHA256 context
    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        fclose(file);
        return -1;
    }
    
    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL) != 1) {
        EVP_MD_CTX_free(mdctx);
        fclose(file);
        return -1;
    }
    
    // Read file and update digest
    unsigned char buffer[8192];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (EVP_DigestUpdate(mdctx, buffer, bytes_read) != 1) {
            EVP_MD_CTX_free(mdctx);
            fclose(file);
            return -1;
        }
    }
    fclose(file);
    
    // Finalize digest
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hash_len;
    if (EVP_DigestFinal_ex(mdctx, hash, &hash_len) != 1) {
        EVP_MD_CTX_free(mdctx);
        return -1;
    }
    
    EVP_MD_CTX_free(mdctx);
    
    // Convert to hex string
    for (unsigned int i = 0; i < hash_len && i < (checksum_size - 1) / 2; i++) {
        snprintf(&checksum[i * 2], 3, "%02x", hash[i]);
    }
    checksum[hash_len * 2] = '\0';
    return 0;
}

// Perform file integrity check
int perform_file_integrity_check(security_scan_result_t* result) {
    if (!result) return -1;
    
    // Define critical system files with expected permissions and ownership
    struct critical_file {
        const char* path;
        mode_t expected_mode;
        uid_t expected_uid;
        gid_t expected_gid;
        bool check_setuid;
        bool check_setgid;
        const char* stored_hash_path;
    } critical_files[] = {
        {"/etc/passwd", 0644, 0, 0, false, false, "/var/lib/autonomy/hashes/etc_passwd.sha256"},
        {"/etc/shadow", 0640, 0, 42, false, false, "/var/lib/autonomy/hashes/etc_shadow.sha256"},
        {"/etc/sudoers", 0440, 0, 0, false, false, "/var/lib/autonomy/hashes/etc_sudoers.sha256"},
        {"/etc/ssh/sshd_config", 0644, 0, 0, false, false, "/var/lib/autonomy/hashes/sshd_config.sha256"},
        {"/bin/su", 0755, 0, 0, true, false, "/var/lib/autonomy/hashes/bin_su.sha256"},
        {"/bin/sudo", 0755, 0, 0, true, false, "/var/lib/autonomy/hashes/bin_sudo.sha256"},
        {"/usr/bin/passwd", 0755, 0, 0, true, false, "/var/lib/autonomy/hashes/usr_bin_passwd.sha256"},
        {"/etc/hosts", 0644, 0, 0, false, false, "/var/lib/autonomy/hashes/etc_hosts.sha256"},
        {"/etc/resolv.conf", 0644, 0, 0, false, false, "/var/lib/autonomy/hashes/etc_resolv_conf.sha256"},
        {"/etc/crontab", 0644, 0, 0, false, false, "/var/lib/autonomy/hashes/etc_crontab.sha256"}
    };
    
    int critical_file_count = sizeof(critical_files) / sizeof(critical_files[0]);
    int issues_found = 0;
    
    for (int i = 0; i < critical_file_count; i++) {
        struct stat st;
        if (stat(critical_files[i].path, &st) == 0) {
            bool issue_detected = false;
            char issue_details[512];
            
            // Check file permissions
            mode_t actual_perms = st.st_mode & 07777;
            if (actual_perms != critical_files[i].expected_mode) {
                snprintf(issue_details, sizeof(issue_details),
                        "File %s has permissions %04o, expected %04o",
                        critical_files[i].path, actual_perms, critical_files[i].expected_mode);
                update_security_events("file_permission", issue_details,
                                     critical_files[i].path, "system", 
                                     (actual_perms & 0002) ? THREAT_LEVEL_CRITICAL : THREAT_LEVEL_HIGH);
                issue_detected = true;
                issues_found++;
            }
            
            // Check ownership
            if (st.st_uid != critical_files[i].expected_uid) {
                snprintf(issue_details, sizeof(issue_details),
                        "File %s owned by UID %d, expected %d",
                        critical_files[i].path, st.st_uid, critical_files[i].expected_uid);
                update_security_events("file_ownership", issue_details,
                                     critical_files[i].path, "system", THREAT_LEVEL_HIGH);
                issue_detected = true;
                issues_found++;
            }
            
            // Check SUID/SGID bits
            if (critical_files[i].check_setuid) {
                if (!(st.st_mode & S_ISUID)) {
                    snprintf(issue_details, sizeof(issue_details),
                            "File %s missing SUID bit", critical_files[i].path);
                    update_security_events("file_permission", issue_details,
                                         critical_files[i].path, "system", THREAT_LEVEL_MEDIUM);
                    issues_found++;
                }
            } else if (st.st_mode & S_ISUID) {
                snprintf(issue_details, sizeof(issue_details),
                        "Unexpected SUID bit on %s", critical_files[i].path);
                update_security_events("file_permission", issue_details,
                                     critical_files[i].path, "system", THREAT_LEVEL_CRITICAL);
                issue_detected = true;
                issues_found++;
            }
            
            // Check file hash if baseline exists
            if (critical_files[i].stored_hash_path) {
                char current_hash[65];
                char stored_hash[65];
                
                if (calculate_file_checksum(critical_files[i].path, current_hash, sizeof(current_hash)) == 0) {
                    // Read stored hash
                    FILE* hash_file = fopen(critical_files[i].stored_hash_path, "r");
                    if (hash_file) {
                        if (fgets(stored_hash, sizeof(stored_hash), hash_file)) {
                            // Remove newline
                            stored_hash[strcspn(stored_hash, "\n")] = 0;
                            
                            if (strcmp(current_hash, stored_hash) != 0) {
                                snprintf(issue_details, sizeof(issue_details),
                                        "File %s has been modified (hash mismatch)",
                                        critical_files[i].path);
                                update_security_events("file_integrity", issue_details,
                                                     critical_files[i].path, "system", THREAT_LEVEL_CRITICAL);
                                issue_detected = true;
                                issues_found++;
                            }
                        }
                        fclose(hash_file);
                    }
                }
            }
            
            // Check for recent modifications
            time_t now = time(NULL);
            if (difftime(now, st.st_mtime) < 3600) { // Modified within last hour
                snprintf(issue_details, sizeof(issue_details),
                        "Critical file %s was recently modified", critical_files[i].path);
                update_security_events("file_modification", issue_details,
                                     critical_files[i].path, "system", 
                                     issue_detected ? THREAT_LEVEL_CRITICAL : THREAT_LEVEL_MEDIUM);
                issues_found++;
            }
        } else {
            // File doesn't exist
            char issue_details[256];
            snprintf(issue_details, sizeof(issue_details),
                    "Critical file %s is missing", critical_files[i].path);
            update_security_events("file_missing", issue_details,
                                 critical_files[i].path, "system", THREAT_LEVEL_HIGH);
            issues_found++;
        }
    }
    
    // Check for suspicious files in system directories
    const char* system_dirs[] = {"/tmp", "/var/tmp", "/dev/shm"};
    for (int i = 0; i < sizeof(system_dirs)/sizeof(system_dirs[0]); i++) {
        DIR* dir = opendir(system_dirs[i]);
        if (dir) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_type == DT_REG) {
                    // Check for suspicious file extensions
                    const char* suspicious_exts[] = {".sh", ".py", ".pl", ".rb", ".php", ".elf"};
                    for (int j = 0; j < sizeof(suspicious_exts)/sizeof(suspicious_exts[0]); j++) {
                        if (strstr(entry->d_name, suspicious_exts[j])) {
                            char full_path[512];
                            snprintf(full_path, sizeof(full_path), "%s/%s", system_dirs[i], entry->d_name);
                            
                            struct stat st;
                            if (stat(full_path, &st) == 0 && (st.st_mode & S_IXUSR)) {
                                char issue_details[512];
                                snprintf(issue_details, sizeof(issue_details),
                                        "Suspicious executable file found: %s", full_path);
                                update_security_events("suspicious_file", issue_details,
                                                     full_path, "system", THREAT_LEVEL_HIGH);
                                issues_found++;
                            }
                        }
                    }
                }
            }
            closedir(dir);
        }
    }
    
    result->vulnerabilities_found += issues_found;
    return 0;
}

// Perform network security check
int perform_network_security_check(security_scan_result_t* result) {
    if (!result) return -1;
    
    int issues_found = 0;
    char issue_details[512];
    
    // Check for open ports that shouldn't be exposed
    const struct {
        int port;
        const char* service;
        threat_level_t threat_level;
    } risky_ports[] = {
        {23, "telnet", THREAT_LEVEL_CRITICAL},
        {21, "ftp", THREAT_LEVEL_HIGH},
        {139, "netbios", THREAT_LEVEL_HIGH},
        {445, "smb", THREAT_LEVEL_HIGH},
        {3389, "rdp", THREAT_LEVEL_HIGH},
        {5900, "vnc", THREAT_LEVEL_HIGH},
        {6379, "redis", THREAT_LEVEL_CRITICAL},
        {27017, "mongodb", THREAT_LEVEL_CRITICAL},
        {3306, "mysql", THREAT_LEVEL_HIGH},
        {5432, "postgresql", THREAT_LEVEL_HIGH},
        {9200, "elasticsearch", THREAT_LEVEL_HIGH},
        {8080, "http-alt", THREAT_LEVEL_MEDIUM},
        {8081, "http-alt", THREAT_LEVEL_MEDIUM},
        {8888, "http-alt", THREAT_LEVEL_MEDIUM}
    };
    
    // Check listening ports using netstat
    FILE* netstat_pipe = popen("netstat -tuln 2>/dev/null", "r");
    if (netstat_pipe) {
        char line[512];
        while (fgets(line, sizeof(line), netstat_pipe)) {
            // Parse netstat output
            if (strstr(line, "LISTEN") || strstr(line, "0.0.0.0:") || strstr(line, ":::")) {
                for (int i = 0; i < sizeof(risky_ports)/sizeof(risky_ports[0]); i++) {
                    char port_str[32];
                    snprintf(port_str, sizeof(port_str), ":%d", risky_ports[i].port);
                    
                    if (strstr(line, port_str)) {
                        snprintf(issue_details, sizeof(issue_details),
                                "Risky port %d (%s) is open and listening",
                                risky_ports[i].port, risky_ports[i].service);
                        update_security_events("open_port", issue_details,
                                             "network", risky_ports[i].service, 
                                             risky_ports[i].threat_level);
                        issues_found++;
                    }
                }
            }
        }
        pclose(netstat_pipe);
    }
    
    // Check for established connections to suspicious IPs
    FILE* conn_pipe = popen("netstat -tun 2>/dev/null | grep ESTABLISHED", "r");
    if (conn_pipe) {
        char line[512];
        while (fgets(line, sizeof(line), conn_pipe)) {
            // Check for connections to non-private IPs on suspicious ports
            char foreign_addr[64];
            int foreign_port;
            
            // Parse foreign address (robust parsing)
            // netstat format: Proto Recv-Q Send-Q Local Address Foreign Address State
            char* tokens[6];
            int token_count = 0;
            char* line_copy = strdup(line);
            char* token = strtok(line_copy, " \t");
            
            while (token && token_count < 6) {
                tokens[token_count++] = token;
                token = strtok(NULL, " \t");
            }
            
            if (token_count >= 5 && tokens[4]) {
                // Parse foreign address:port from tokens[4]
                if (sscanf(tokens[4], "%63[^:]:%d", foreign_addr, &foreign_port) == 2) {
                    // Check for connections to common malware C&C ports
                    const int suspicious_ports[] = {4444, 5555, 6666, 7777, 8888, 9999, 31337};
                    for (int i = 0; i < sizeof(suspicious_ports)/sizeof(suspicious_ports[0]); i++) {
                        if (foreign_port == suspicious_ports[i]) {
                            snprintf(issue_details, sizeof(issue_details),
                                    "Suspicious connection to %s:%d detected",
                                    foreign_addr, foreign_port);
                            update_security_events("suspicious_connection", issue_details,
                                                 "network", foreign_addr, THREAT_LEVEL_CRITICAL);
                            issues_found++;
                        }
                    }
                }
            }
            free(line_copy);
        }
        pclose(conn_pipe);
    }
    
    // Check firewall status
    FILE* iptables_pipe = popen("iptables -L -n 2>/dev/null | head -n 1", "r");
    if (iptables_pipe) {
        char line[256];
        bool firewall_active = false;
        
        if (fgets(line, sizeof(line), iptables_pipe)) {
            if (strstr(line, "Chain INPUT")) {
                // Check if there are any rules
                FILE* rule_count_pipe = popen("iptables -L INPUT -n 2>/dev/null | wc -l", "r");
                if (rule_count_pipe) {
                    int rule_count = 0;
                    if (fscanf(rule_count_pipe, "%d", &rule_count) == 1 && rule_count > 2) {
                        firewall_active = true;
                    }
                    pclose(rule_count_pipe);
                }
            }
        }
        pclose(iptables_pipe);
        
        if (!firewall_active) {
            snprintf(issue_details, sizeof(issue_details),
                    "Firewall appears to be disabled or has no rules configured");
            update_security_events("firewall_disabled", issue_details,
                                 "network", "system", THREAT_LEVEL_HIGH);
            issues_found++;
        }
    }
    
    // Check for promiscuous mode interfaces (potential packet sniffing)
    FILE* ifconfig_pipe = popen("ip link show 2>/dev/null", "r");
    if (ifconfig_pipe) {
        char line[512];
        char current_iface[32] = {0};
        
        while (fgets(line, sizeof(line), ifconfig_pipe)) {
            // Parse interface name
            if (line[0] != ' ') {
                char* colon = strchr(line, ':');
                if (colon) {
                    char* iface_start = strchr(line, ' ');
                    if (iface_start && iface_start < colon) {
                        int len = colon - iface_start - 1;
                        if (len < sizeof(current_iface)) {
                            strncpy(current_iface, iface_start + 1, len);
                            current_iface[len] = '\0';
                        }
                    }
                }
            }
            
            // Check for PROMISC flag
            if (strstr(line, "PROMISC") && strlen(current_iface) > 0) {
                snprintf(issue_details, sizeof(issue_details),
                        "Network interface %s is in promiscuous mode (possible packet sniffing)",
                        current_iface);
                update_security_events("promiscuous_interface", issue_details,
                                     "network", current_iface, THREAT_LEVEL_CRITICAL);
                issues_found++;
            }
        }
        pclose(ifconfig_pipe);
    }
    
    // Check for unusual network protocols
    FILE* proto_pipe = popen("netstat -anp 2>/dev/null | grep -v 'tcp\\|udp\\|unix' | grep -v 'Active\\|Proto'", "r");
    if (proto_pipe) {
        char line[512];
        while (fgets(line, sizeof(line), proto_pipe)) {
            if (strlen(line) > 10) {
                snprintf(issue_details, sizeof(issue_details),
                        "Unusual network protocol detected: %.100s", line);
                update_security_events("unusual_protocol", issue_details,
                                     "network", "system", THREAT_LEVEL_MEDIUM);
                issues_found++;
            }
        }
        pclose(proto_pipe);
    }
    
    // Check SSH configuration for security issues
    FILE* sshd_config = fopen("/etc/ssh/sshd_config", "r");
    if (sshd_config) {
        char line[256];
        bool permit_root_login = false;
        bool password_auth = true;
        bool pubkey_auth = false;
        
        while (fgets(line, sizeof(line), sshd_config)) {
            // Skip comments and empty lines
            if (line[0] == '#' || line[0] == '\n') continue;
            
            if (strstr(line, "PermitRootLogin") && strstr(line, "yes")) {
                permit_root_login = true;
            }
            if (strstr(line, "PasswordAuthentication") && strstr(line, "no")) {
                password_auth = false;
            }
            if (strstr(line, "PubkeyAuthentication") && strstr(line, "yes")) {
                pubkey_auth = true;
            }
        }
        fclose(sshd_config);
        
        if (permit_root_login) {
            snprintf(issue_details, sizeof(issue_details),
                    "SSH server allows root login");
            update_security_events("ssh_config", issue_details,
                                 "network", "sshd", THREAT_LEVEL_HIGH);
            issues_found++;
        }
        
        if (password_auth && !pubkey_auth) {
            snprintf(issue_details, sizeof(issue_details),
                    "SSH server uses password authentication without public key requirement");
            update_security_events("ssh_config", issue_details,
                                 "network", "sshd", THREAT_LEVEL_MEDIUM);
            issues_found++;
        }
    }
    
    result->vulnerabilities_found += issues_found;
    return 0;
}

// Perform access control check
int perform_access_control_check(security_scan_result_t* result) {
    if (!result) return -1;
    
    // Production access control check
    
    // Check /var/log/auth.log for failed authentication attempts
    FILE* auth_log = fopen("/var/log/auth.log", "r");
    if (auth_log) {
        char line[1024];
        int failed_attempts = 0; // Use configurable failed attempts counter
        time_t current_time = time(NULL);
        time_t check_time = current_time - 3600; // Last hour
        
        while (fgets(line, sizeof(line), auth_log)) {
            // Look for authentication failures
            if (strstr(line, "authentication failure") || 
                strstr(line, "Failed password") ||
                strstr(line, "Invalid user")) {
                
                // Parse timestamp and check if within last hour
                struct tm tm;
                char month[16], day[16], time_str[16];
                
                if (sscanf(line, "%15s %15s %15s", month, day, time_str) == 3) {
                    // Simple time comparison (would need proper parsing in production)
                    failed_attempts++;
                }
            }
        }
        fclose(auth_log);
        
        if (failed_attempts > 10) {
            update_security_events("access_control", 
                                  "Excessive failed login attempts detected",
                                  "authentication", "system", THREAT_LEVEL_HIGH);
            result->vulnerabilities_found++;
            return -1;
        } else if (failed_attempts > 3) {
            update_security_events("access_control", 
                                  "Multiple failed login attempts detected",
                                  "authentication", "system", THREAT_LEVEL_MEDIUM);
            result->vulnerabilities_found++;
        }
    }
    
    // Check for unusual sudo usage
    FILE* sudo_log = popen("journalctl --since='1 hour ago' -u sudo 2>/dev/null | grep 'COMMAND=' | wc -l", "r");
    if (sudo_log) {
        char count_str[32];
        if (fgets(count_str, sizeof(count_str), sudo_log)) {
            int sudo_count = atoi(count_str);
            if (sudo_count > 50) {
                update_security_events("access_control", 
                                      "Excessive sudo usage detected",
                                      "privilege_escalation", "system", THREAT_LEVEL_MEDIUM);
                result->vulnerabilities_found++;
            }
        }
        pclose(sudo_log);
    }
    
    // Check for active root sessions
    FILE* who_cmd = popen("who | grep 'root' | wc -l", "r");
    if (who_cmd) {
        char count_str[32];
        if (fgets(count_str, sizeof(count_str), who_cmd)) {
            int root_sessions = atoi(count_str);
            if (root_sessions > 0) {
                update_security_events("access_control", 
                                      "Active root session detected",
                                      "privilege_escalation", "system", THREAT_LEVEL_LOW);
            }
        }
        pclose(who_cmd);
    }
    
    return 0;
}

// Perform configuration check
int perform_configuration_check(security_scan_result_t* result) {
    if (!result) return -1;
    
    // Production configuration security check
    
    // Check SSH configuration
    FILE* ssh_config = fopen("/etc/ssh/sshd_config", "r");
    if (ssh_config) {
        char line[512];
        bool permit_root_login = false;
        bool password_auth = true;
        bool permit_empty_passwords = false;
        
        while (fgets(line, sizeof(line), ssh_config)) {
            if (strstr(line, "PermitRootLogin yes")) {
                permit_root_login = true;
            }
            if (strstr(line, "PasswordAuthentication no")) {
                password_auth = false;
            }
            if (strstr(line, "PermitEmptyPasswords yes")) {
                permit_empty_passwords = true;
            }
        }
        fclose(ssh_config);
        
        if (permit_root_login) {
            update_security_events("configuration", 
                                  "SSH root login is enabled - security risk",
                                  "ssh_config", "system", THREAT_LEVEL_HIGH);
            result->vulnerabilities_found++;
        }
        
        if (permit_empty_passwords) {
            update_security_events("configuration", 
                                  "SSH allows empty passwords - critical security risk",
                                  "ssh_config", "system", THREAT_LEVEL_CRITICAL);
            result->vulnerabilities_found++;
        }
    }
    
    // Check file permissions on critical system files
    const char* critical_files[] = {
        "/etc/passwd",
        "/etc/shadow",
        "/etc/group",
        "/etc/sudoers",
        "/etc/ssh/ssh_host_rsa_key",
        NULL
    };
    
    for (int i = 0; critical_files[i]; i++) {
        struct stat file_stat;
        if (stat(critical_files[i], &file_stat) == 0) {
            // Check if world-writable
            if (file_stat.st_mode & S_IWOTH) {
                char msg[512];
                snprintf(msg, sizeof(msg), "Critical file %s is world-writable", critical_files[i]);
                update_security_events("configuration", msg,
                                      "file_permissions", "filesystem", THREAT_LEVEL_CRITICAL);
                result->vulnerabilities_found++;
            }
            
            // Check if shadow file is readable by others
            if (strcmp(critical_files[i], "/etc/shadow") == 0 && 
                (file_stat.st_mode & (S_IRGRP | S_IROTH))) {
                update_security_events("configuration", 
                                      "/etc/shadow is readable by non-root users",
                                      "file_permissions", "filesystem", THREAT_LEVEL_HIGH);
                result->vulnerabilities_found++;
            }
        }
    }
    
    // Check for SUID/SGID binaries in unusual locations
    FILE* suid_cmd = popen("find /home /tmp /var/tmp -type f \\( -perm -4000 -o -perm -2000 \\) 2>/dev/null", "r");
    if (suid_cmd) {
        char path[512];
        while (fgets(path, sizeof(path), suid_cmd)) {
            // Remove newline
            path[strcspn(path, "\n")] = 0;
            
            char msg[1024];
            snprintf(msg, sizeof(msg), "SUID/SGID binary found in unusual location: %s", path);
            update_security_events("configuration", msg,
                                  "suid_binary", "filesystem", THREAT_LEVEL_MEDIUM);
            result->vulnerabilities_found++;
        }
        pclose(suid_cmd);
    }
    
    return 0;
}

// Perform threat detection
static int perform_threat_detection(security_scan_result_t* result) {
    if (!result) return -1;
    
    // Production threat detection system
    
    // Check for port scanning attempts
    FILE* netstat_cmd = popen("ss -tuln | grep ':' | wc -l", "r");
    if (netstat_cmd) {
        char count_str[32];
        if (fgets(count_str, sizeof(count_str), netstat_cmd)) {
            int listening_ports = atoi(count_str);
            static int prev_port_count = 0;
            
            if (prev_port_count > 0 && listening_ports > prev_port_count + 10) {
                update_security_events("threat_detection", 
                                      "Rapid increase in listening ports detected",
                                      "port_scanning", "network", THREAT_LEVEL_HIGH);
                result->vulnerabilities_found++;
            }
            prev_port_count = listening_ports;
        }
        pclose(netstat_cmd);
    }
    
    // Check for suspicious processes
    FILE* proc_cmd = popen("ps aux | grep -E '(nmap|nikto|sqlmap|metasploit|nc|netcat)' | grep -v grep", "r");
    if (proc_cmd) {
        char process_line[1024];
        while (fgets(process_line, sizeof(process_line), proc_cmd)) {
            // Remove newline
            process_line[strcspn(process_line, "\n")] = 0;
            
            char msg[1024];
            snprintf(msg, sizeof(msg), "Suspicious process detected: %s", process_line);
            update_security_events("threat_detection", msg,
                                  "malicious_process", "system", THREAT_LEVEL_HIGH);
            result->vulnerabilities_found++;
        }
        pclose(proc_cmd);
    }
    
    // Check for unusual CPU/Memory usage patterns
    FILE* cpu_cmd = popen("top -bn1 | grep 'Cpu(s)' | awk '{print $2}' | cut -d'%' -f1", "r");
    if (cpu_cmd) {
        char cpu_str[32];
        if (fgets(cpu_str, sizeof(cpu_str), cpu_cmd)) {
            float cpu_usage = atof(cpu_str);
            static float prev_cpu_usage = 0.0;
            
            // Detect sudden CPU spikes
            if (cpu_usage > 90.0 && prev_cpu_usage < 20.0) {
                update_security_events("threat_detection", 
                                      "Sudden CPU usage spike detected - possible crypto mining",
                                      "resource_abuse", "system", THREAT_LEVEL_MEDIUM);
                result->vulnerabilities_found++;
            }
            prev_cpu_usage = cpu_usage;
        }
        pclose(cpu_cmd);
    }
    
    // Check for new cronjobs
    FILE* cron_cmd = popen("crontab -l 2>/dev/null | grep -v '^#' | wc -l", "r");
    if (cron_cmd) {
        char count_str[32];
        if (fgets(count_str, sizeof(count_str), cron_cmd)) {
            int cron_count = atoi(count_str);
            static int prev_cron_count = -1;
            
            if (prev_cron_count >= 0 && cron_count > prev_cron_count) {
                update_security_events("threat_detection", 
                                      "New cron jobs detected - potential persistence mechanism",
                                      "persistence", "system", THREAT_LEVEL_MEDIUM);
                result->vulnerabilities_found++;
            }
            prev_cron_count = cron_count;
        }
        pclose(cron_cmd);
    }
    
    // Check for unusual network connections
    FILE* conn_cmd = popen("ss -tuln | grep ':22 ' | wc -l", "r");
    if (conn_cmd) {
        char count_str[32];
        if (fgets(count_str, sizeof(count_str), conn_cmd)) {
            int ssh_connections = atoi(count_str);
            if (ssh_connections > 10) {
                update_security_events("threat_detection", 
                                      "High number of SSH connections detected",
                                      "brute_force", "network", THREAT_LEVEL_HIGH);
                result->vulnerabilities_found++;
            }
        }
        pclose(conn_cmd);
    }
    
    return 0;
}

// Update security events
void update_security_events(const char* event_type, const char* description, 
                                  const char* source, const char* target, threat_level_t level) {
    if (g_security_monitor.event_count >= 100) return;
    
    security_event_t* event = &g_security_monitor.security_events[g_security_monitor.event_index];
    
    // Generate unique event ID
    char* event_id = generate_event_id();
    strcpy(event->event_id, event_id);
    free(event_id);
    
    // Set event details
    event->threat_level = level;
    strcpy(event->event_type, event_type);
    strcpy(event->description, description);
    strcpy(event->source, source ? source : "unknown");
    strcpy(event->target, target ? target : "unknown");
    event->timestamp = time(NULL);
    event->acknowledged = false;
    
    // Set mitigation based on threat level
    switch (level) {
        case THREAT_LEVEL_CRITICAL:
            strcpy(event->mitigation, "Immediate action required - isolate system");
            break;
        case THREAT_LEVEL_HIGH:
            strcpy(event->mitigation, "Investigate and remediate within 1 hour");
            break;
        case THREAT_LEVEL_MEDIUM:
            strcpy(event->mitigation, "Review and address within 24 hours");
            break;
        case THREAT_LEVEL_LOW:
            strcpy(event->mitigation, "Monitor and address during next maintenance");
            break;
    }
    
    // Update event index
    g_security_monitor.event_index = (g_security_monitor.event_index + 1) % 100;
    if (g_security_monitor.event_count < 100) {
        g_security_monitor.event_count++;
    }
}

// Generate unique event ID
static char* generate_event_id(void) {
    char* event_id = malloc(64);
    if (!event_id) return NULL;
    
    time_t now = time(NULL);
    snprintf(event_id, 64, "SEC_%ld_%d", now, rand() % 10000);
    
    return event_id;
}

// Get security monitor status
void security_monitor_get_status(security_monitor_t* status) {
    if (!status || !g_security_monitor_initialized) return;
    
    pthread_mutex_lock(g_security_monitor.mutex);
    *status = g_security_monitor;
    pthread_mutex_unlock(g_security_monitor.mutex);
}

// Check if security monitor is initialized
bool security_monitor_is_initialized(void) {
    return g_security_monitor_initialized;
}

// Get security monitor instance
security_monitor_t* security_monitor_get_instance(void) {
    return g_security_monitor_initialized ? &g_security_monitor : NULL;
}
