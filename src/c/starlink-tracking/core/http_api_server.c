#include "starlink_tracker_standalone.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <microhttpd.h>
#include <json-c/json.h>

// Global tracker reference for HTTP server
static standalone_tracker_t *g_tracker_ref = NULL;

// HTTP response structure
struct api_response {
    char *data;
    size_t size;
    const char *content_type;
};

// Connection context for each HTTP request
typedef struct {
    int request_type; // Example: 0 for status, 1 for predictions
    // Add other per-request data here if needed
} connection_context_t;

// Create JSON response for status endpoint
static struct api_response create_status_response(void) {
    struct api_response response = {0};
    
    if (!g_tracker_ref) {
        response.data = strdup("{\"error\": \"Tracker not initialized\"}");
        response.size = strlen(response.data);
        response.content_type = "application/json";
        return response;
    }
    
    standalone_stats_t stats = standalone_tracker_get_stats(g_tracker_ref);
    
    json_object *root = json_object_new_object();
    json_object *status = json_object_new_string(standalone_tracker_is_monitoring(g_tracker_ref) ? "monitoring" : "idle");
    json_object *visible_sats = json_object_new_int(stats.visible_satellites);
    json_object *unobstructed_sats = json_object_new_int(stats.unobstructed_satellites);
    json_object *obstruction_pct = json_object_new_double(stats.obstruction_percentage);
    json_object *accuracy = json_object_new_double(stats.accuracy_percentage);
    json_object *last_update = json_object_new_int64((int64_t)stats.last_update);
    
    json_object_object_add(root, "status", status);
    json_object_object_add(root, "visible_satellites", visible_sats);
    json_object_object_add(root, "unobstructed_satellites", unobstructed_sats);
    json_object_object_add(root, "obstruction_percentage", obstruction_pct);
    json_object_object_add(root, "accuracy_percentage", accuracy);
    json_object_object_add(root, "last_update", last_update);
    
    const char *json_string = json_object_to_json_string(root);
    response.data = strdup(json_string);
    response.size = strlen(response.data);
    response.content_type = "application/json";
    
    json_object_put(root);
    
    return response;
}

// Create JSON response for predictions endpoint
static struct api_response create_predictions_response(void) {
    struct api_response response = {0};
    
    if (!g_tracker_ref) {
        response.data = strdup("{\"error\": \"Tracker not initialized\"}");
        response.size = strlen(response.data);
        response.content_type = "application/json";
        return response;
    }
    
    standalone_outage_prediction_t *predictions;
    int num_predictions = standalone_tracker_get_predictions(g_tracker_ref, &predictions);
    
    json_object *root = json_object_new_object();
    json_object *count = json_object_new_int(num_predictions);
    json_object_object_add(root, "count", count);
    
    if (num_predictions > 0) {
        json_object *predictions_array = json_object_new_array();
        
        for (int i = 0; i < num_predictions; i++) {
            json_object *pred_obj = json_object_new_object();
            
            json_object_object_add(pred_obj, "start_time", json_object_new_int64(predictions[i].start_time));
            json_object_object_add(pred_obj, "end_time", json_object_new_int64(predictions[i].end_time));
            json_object_object_add(pred_obj, "duration_seconds", json_object_new_int(predictions[i].duration_seconds));
            json_object_object_add(pred_obj, "risk_level", json_object_new_int(predictions[i].risk_level));
            json_object_object_add(pred_obj, "description", json_object_new_string(predictions[i].description));
            json_object_object_add(pred_obj, "predicted_available_sats", json_object_new_int(predictions[i].predicted_available_sats));
            json_object_object_add(pred_obj, "confidence_score", json_object_new_double(predictions[i].confidence_score));
            
            json_object_array_add(predictions_array, pred_obj);
        }
        
        json_object_object_add(root, "predictions", predictions_array);
        standalone_tracker_free_predictions(predictions, num_predictions);
    }
    
    const char *json_string = json_object_to_json_string(root);
    response.data = strdup(json_string);
    response.size = strlen(response.data);
    response.content_type = "application/json";
    
    json_object_put(root);
    
    return response;
}

// Serve static files
static struct api_response starlink_serve_static_file(const char *filename) {
    struct api_response response = {0};
    
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
    
    // Try to find file in web directory
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "web/%s", filename);
    
    FILE *file = fopen(filepath, "r");
    if (!file) {
        // Try current directory
        file = fopen(filename, "r");
    }
    
    if (!file) {
        response.data = strdup("File not found");
        response.size = strlen(response.data);
        response.content_type = "text/plain";
        return response;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    // Read file
    response.data = malloc(file_size + 1);
    response.size = fread(response.data, 1, file_size, file);
    response.data[response.size] = '\0';
    
    fclose(file);
    
    return response;
}

// HTTP request handler
static enum MHD_Result starlink_request_handler(void *cls,
                                     struct MHD_Connection *connection,
                                     const char *url,
                                     const char *method,
                                     const char *version,
                                     const char *upload_data,
                                     size_t *upload_data_size,
                                     void **con_cls) {
    
    if (*con_cls == NULL) {
        // New connection, allocate context
        connection_context_t *context = calloc(1, sizeof(connection_context_t));
        if (context == NULL) {
            return MHD_NO;
        }
        *con_cls = context;
        return MHD_YES;
    }
    
    struct MHD_Response *response;
    enum MHD_Result ret;
    struct api_response api_resp = {0};
    
    if (*upload_data_size != 0) {
        return MHD_NO;
    }
    
    // Free connection context
    if (*con_cls) {
        free(*con_cls);
        *con_cls = NULL;
    }
    
    // Route requests
    if (strcmp(url, "/") == 0 || strcmp(url, "/index.html") == 0) {
        api_resp = starlink_serve_static_file("index.html");
    } else if (strcmp(url, "/starlink_visualization.js") == 0) {
        api_resp = starlink_serve_static_file("starlink_visualization.js");
    } else if (strcmp(url, "/api/status") == 0) {
        api_resp = create_status_response();
    } else if (strcmp(url, "/api/predictions") == 0) {
        api_resp = create_predictions_response();
    } else if (strcmp(url, "/api/satellites") == 0) {
        // Simplified satellites response
        api_resp.data = strdup("{\"count\": 0, \"satellites\": []}");
        api_resp.size = strlen(api_resp.data);
        api_resp.content_type = "application/json";
    } else {
        api_resp.data = strdup("404 - Not Found");
        api_resp.size = strlen(api_resp.data);
        api_resp.content_type = "text/plain";
    }
    
    // Create MHD response
    response = MHD_create_response_from_buffer(api_resp.size, api_resp.data, MHD_RESPMEM_MUST_FREE);
    if (!response) {
        if (api_resp.data) free(api_resp.data);
        return MHD_NO;
    }
    
    // Set headers
    MHD_add_response_header(response, "Content-Type", api_resp.content_type);
    MHD_add_response_header(response, "Access-Control-Allow-Origin", "*");
    MHD_add_response_header(response, "Access-Control-Allow-Methods", "GET, OPTIONS");
    
    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);
    
    return ret;
}

// Start HTTP API server
struct MHD_Daemon* start_http_api_server(standalone_tracker_t *tracker, int port) {
    if (!tracker) {
        return NULL;
    }
    
    g_tracker_ref = tracker;
    
    struct MHD_Daemon *daemon = MHD_start_daemon(
        MHD_USE_THREAD_PER_CONNECTION,
        port,
        NULL, NULL,
        &starlink_request_handler, NULL,
        MHD_OPTION_END
    );
    
    if (daemon) {
        printf("🌐 HTTP API server started on port %d\n", port);
    } else {
        g_tracker_ref = NULL;
    }
    
    return daemon;
}

// Stop HTTP API server
void stop_http_api_server(struct MHD_Daemon *daemon) {
    if (daemon) {
        MHD_stop_daemon(daemon);
        g_tracker_ref = NULL;
        printf("🌐 HTTP API server stopped\n");
    }
}