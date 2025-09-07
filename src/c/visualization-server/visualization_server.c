#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <microhttpd.h>
#include <json-c/json.h>
#include <libubus.h>
#include <libubox/blobmsg_json.h>

// HTTP server configuration
#define DEFAULT_PORT 8080
#define MAX_RESPONSE_SIZE 65536

// Global UBUS context
static struct ubus_context *ubus_ctx = NULL;

// HTTP response structure
struct http_response {
    char *data;
    size_t size;
    const char *content_type;
};

// Initialize UBUS connection
static int init_ubus(void) {
    ubus_ctx = ubus_connect(NULL);
    if (!ubus_ctx) {
        fprintf(stderr, "Failed to connect to UBUS\n");
        return -1;
    }
    return 0;
}

// Call UBUS method and return JSON response
static char* call_ubus_method(const char *object, const char *method) {
    if (!ubus_ctx) {
        return NULL;
    }
    
    uint32_t id;
    if (ubus_lookup_id(ubus_ctx, object, &id) != 0) {
        return NULL;
    }
    
    struct blob_buf req = {};
    blob_buf_init(&req, 0);
    
    struct blob_buf resp = {};
    blob_buf_init(&resp, 0);
    
    int ret = ubus_invoke(ubus_ctx, id, method, req.head, NULL, NULL, 5000);
    
    blob_buf_free(&req);
    
    if (ret != 0) {
        blob_buf_free(&resp);
        return NULL;
    }
    
    // Convert blob to JSON string
    char *json_str = blobmsg_format_json(resp.head, true);
    blob_buf_free(&resp);
    
    return json_str;
}

// Serve static files
static struct http_response viz_serve_static_file(const char *filename) {
    struct http_response response = {0};
    
    // Determine content type
    if (strstr(filename, ".html")) {
        response.content_type = "text/html";
    } else if (strstr(filename, ".js")) {
        response.content_type = "application/javascript";
    } else if (strstr(filename, ".css")) {
        response.content_type = "text/css";
    } else {
        response.content_type = "text/plain";
    }
    
    // Read file
    FILE *file = fopen(filename, "r");
    if (!file) {
        response.data = strdup("File not found");
        response.size = strlen(response.data);
        return response;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Read file content
    response.data = malloc(file_size + 1);
    response.size = fread(response.data, 1, file_size, file);
    response.data[response.size] = '\0';
    
    fclose(file);
    
    return response;
}

// HTTP request handler
static enum MHD_Result viz_request_handler(void *cls,
                                     struct MHD_Connection *connection,
                                     const char *url,
                                     const char *method,
                                     const char *version,
                                     const char *upload_data,
                                     size_t *upload_data_size,
                                     void **con_cls) {
    
    // Connection context structure for proper request handling
    typedef struct {
        bool is_new_connection;
        time_t request_time;
        char client_ip[INET6_ADDRSTRLEN];
    } connection_context_t;
    
    struct MHD_Response *response;
    enum MHD_Result ret;
    struct http_response http_resp = {0};
    
    // Handle new connection
    if (*con_cls == NULL) {
        // Allocate connection context for this request
        connection_context_t *context = calloc(1, sizeof(connection_context_t));
        if (context == NULL) {
            // Memory allocation failed
            return MHD_NO;
        }
        
        context->is_new_connection = true;
        context->request_time = time(NULL);
        
        // Get client IP address
        const union MHD_ConnectionInfo *info = MHD_get_connection_info(connection,
                                                    MHD_CONNECTION_INFO_CLIENT_ADDRESS);
        if (info && info->client_addr) {
            struct sockaddr *addr = info->client_addr;
            if (addr->sa_family == AF_INET) {
                struct sockaddr_in *addr_in = (struct sockaddr_in *)addr;
                inet_ntop(AF_INET, &addr_in->sin_addr, context->client_ip, 
                         sizeof(context->client_ip));
            } else if (addr->sa_family == AF_INET6) {
                struct sockaddr_in6 *addr_in6 = (struct sockaddr_in6 *)addr;
                inet_ntop(AF_INET6, &addr_in6->sin6_addr, context->client_ip,
                         sizeof(context->client_ip));
            }
        }
        
        *con_cls = context;
        return MHD_YES;  // Continue processing
    }
    
    connection_context_t *context = (connection_context_t *)*con_cls;
    
    // Handle upload data if present
    if (*upload_data_size != 0) {
        // For now, we don't support POST data
        *upload_data_size = 0;
        return MHD_YES;
    }
    
    // Log the request
    printf("[%s] %s request for %s\n", context->client_ip, method, url);
    
    // Route requests
    if (strcmp(url, "/") == 0 || strcmp(url, "/index.html") == 0) {
        http_resp = viz_serve_static_file("index.html");
    } else if (strcmp(url, "/starlink_visualization.js") == 0) {
        http_resp = viz_serve_static_file("starlink_visualization.js");
    } else if (strncmp(url, "/api/", 5) == 0) {
        // API endpoints
        const char *api_path = url + 5; // Skip "/api/"
        
        if (strcmp(api_path, "status") == 0) {
            char *ubus_response = call_ubus_method("starlink_tracker", "status");
            if (ubus_response) {
                http_resp.data = ubus_response;
                http_resp.size = strlen(ubus_response);
                http_resp.content_type = "application/json";
            } else {
                http_resp.data = strdup("{\"error\": \"UBUS call failed\"}");
                http_resp.size = strlen(http_resp.data);
                http_resp.content_type = "application/json";
            }
        } else if (strcmp(api_path, "satellites") == 0) {
            char *ubus_response = call_ubus_method("starlink_tracker", "satellites");
            if (ubus_response) {
                http_resp.data = ubus_response;
                http_resp.size = strlen(ubus_response);
                http_resp.content_type = "application/json";
            } else {
                http_resp.data = strdup("{\"error\": \"UBUS call failed\"}");
                http_resp.size = strlen(http_resp.data);
                http_resp.content_type = "application/json";
            }
        } else if (strcmp(api_path, "predictions") == 0) {
            char *ubus_response = call_ubus_method("starlink_tracker", "predictions");
            if (ubus_response) {
                http_resp.data = ubus_response;
                http_resp.size = strlen(ubus_response);
                http_resp.content_type = "application/json";
            } else {
                http_resp.data = strdup("{\"error\": \"UBUS call failed\"}");
                http_resp.size = strlen(http_resp.data);
                http_resp.content_type = "application/json";
            }
        } else {
            http_resp.data = strdup("{\"error\": \"API endpoint not found\"}");
            http_resp.size = strlen(http_resp.data);
            http_resp.content_type = "application/json";
        }
    } else {
        http_resp.data = strdup("404 - Not Found");
        http_resp.size = strlen(http_resp.data);
        http_resp.content_type = "text/plain";
    }
    
    // Create MHD response
    response = MHD_create_response_from_buffer(http_resp.size, http_resp.data, MHD_RESPMEM_MUST_FREE);
    
    if (!response) {
        if (http_resp.data) free(http_resp.data);
        return MHD_NO;
    }
    
    // Set content type
    MHD_add_response_header(response, "Content-Type", http_resp.content_type);
    MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
    MHD_add_response_header(response, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    MHD_add_response_header(response, "Access-Control-Allow-Headers", "Content-Type");
    
    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    
    // Clean up connection context
    if (context) {
        free(context);
        *con_cls = NULL;
    }
    
    return ret;
}

int viz_main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "Invalid port number\n");
            return 1;
        }
    }
    
    printf("🛰️ Starlink Visualization Server\n");
    printf("================================\n\n");
    
    // Initialize UBUS
    printf("🔌 Connecting to UBUS...\n");
    if (init_ubus() != 0) {
        fprintf(stderr, "❌ Failed to initialize UBUS connection\n");
        fprintf(stderr, "   Make sure autonomy daemon is running\n");
        return 1;
    }
    printf("✅ UBUS connected successfully\n\n");
    
    // Start HTTP server
    printf("🌐 Starting HTTP server on port %d...\n", port);
    
    struct MHD_Daemon *daemon = MHD_start_daemon(
        MHD_USE_THREAD_PER_CONNECTION,
        port,
        NULL, NULL,
        &viz_request_handler, NULL,
        MHD_OPTION_END
    );
    
    if (!daemon) {
        fprintf(stderr, "❌ Failed to start HTTP server\n");
        ubus_free(ubus_ctx);
        return 1;
    }
    
    printf("✅ Server started successfully!\n\n");
    printf("🌐 Open your browser and navigate to:\n");
    printf("   http://localhost:%d\n", port);
    printf("   or\n");
    printf("   http://192.168.1.1:%d (if running on router)\n\n", port);
    printf("📊 API endpoints available:\n");
    printf("   http://localhost:%d/api/status\n", port);
    printf("   http://localhost:%d/api/satellites\n", port);
    printf("   http://localhost:%d/api/predictions\n\n", port);
    printf("Press Ctrl+C to stop the server...\n");
    
    // Wait for interrupt
    getchar();
    
    // Cleanup
    printf("\n🛑 Stopping server...\n");
    MHD_stop_daemon(daemon);
    
    if (ubus_ctx) {
        ubus_free(ubus_ctx);
    }
    
    printf("✅ Server stopped successfully\n");
    return 0;
}

// Main function for visualization server executable
int main(int argc, char *argv[]) {
    return viz_main(argc, argv);
}