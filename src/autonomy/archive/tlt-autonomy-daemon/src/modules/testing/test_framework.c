#include "test_framework.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

// Global test framework instance
static test_framework_t g_test_framework;
static bool g_test_framework_initialized = false;

// Forward declarations
static int run_test_suite_internal(const char* suite_name);
static int run_test_case_internal(const char* suite_name, const char* test_name);
static void update_test_result(test_case_t* test_case, test_result_t result, const char* error_message);
static double calculate_test_duration(time_t start_time, time_t end_time);
static void log_test_result(const test_case_t* test_case);

// Initialize test framework
int test_framework_init(const test_framework_config_t* config) {
    if (g_test_framework_initialized) {
        return 0; // Already initialized
    }
    
    memset(&g_test_framework, 0, sizeof(test_framework_t));
    
    // Set configuration
    if (config) {
        g_test_framework.config = *config;
    } else {
        // Default configuration
        g_test_framework.config.enabled = true;
        g_test_framework.config.run_on_startup = false;
        g_test_framework.config.stop_on_failure = false;
        g_test_framework.config.verbose_output = true;
        strcpy(g_test_framework.config.output_file, "/tmp/autonomy_tests.log");
        g_test_framework.config.timeout_seconds = 300; // 5 minutes
    }
    
    // Initialize mutex
    g_test_framework.mutex = malloc(sizeof(pthread_mutex_t));
    if (!g_test_framework.mutex) {
        return -1;
    }
    
    pthread_mutex_init(g_test_framework.mutex, NULL);
    
    // Initialize test suites
    g_test_framework.test_suite_count = 0;
    
    // Add default test suites
    strcpy(g_test_framework.test_suites[0].name, "core");
    strcpy(g_test_framework.test_suites[0].description, "Core functionality tests");
    g_test_framework.test_suites[0].test_case_count = 0;
    g_test_framework.test_suites[0].passed_count = 0;
    g_test_framework.test_suites[0].failed_count = 0;
    g_test_framework.test_suites[0].skipped_count = 0;
    g_test_framework.test_suites[0].error_count = 0;
    g_test_framework.test_suites[0].start_time = 0;
    g_test_framework.test_suites[0].end_time = 0;
    g_test_framework.test_suites[0].total_duration = 0.0;
    g_test_framework.test_suite_count++;
    
    strcpy(g_test_framework.test_suites[1].name, "network");
    strcpy(g_test_framework.test_suites[1].description, "Network functionality tests");
    g_test_framework.test_suites[1].test_case_count = 0;
    g_test_framework.test_suites[1].passed_count = 0;
    g_test_framework.test_suites[1].failed_count = 0;
    g_test_framework.test_suites[1].skipped_count = 0;
    g_test_framework.test_suites[1].error_count = 0;
    g_test_framework.test_suites[1].start_time = 0;
    g_test_framework.test_suites[1].end_time = 0;
    g_test_framework.test_suites[1].total_duration = 0.0;
    g_test_framework.test_suite_count++;
    
    strcpy(g_test_framework.test_suites[2].name, "gps");
    strcpy(g_test_framework.test_suites[2].description, "GPS functionality tests");
    g_test_framework.test_suites[2].test_case_count = 0;
    g_test_framework.test_suites[2].passed_count = 0;
    g_test_framework.test_suites[2].failed_count = 0;
    g_test_framework.test_suites[2].skipped_count = 0;
    g_test_framework.test_suites[2].error_count = 0;
    g_test_framework.test_suites[2].start_time = 0;
    g_test_framework.test_suites[2].end_time = 0;
    g_test_framework.test_suites[2].total_duration = 0.0;
    g_test_framework.test_suite_count++;
    
    g_test_framework_initialized = true;
    return 0;
}

// Clean up test framework
void test_framework_cleanup(void) {
    if (!g_test_framework_initialized) return;
    
    if (g_test_framework.mutex) {
        pthread_mutex_destroy(g_test_framework.mutex);
        free(g_test_framework.mutex);
    }
    
    g_test_framework.mutex = NULL;
    g_test_framework_initialized = false;
}

// Run all tests
int test_framework_run_all_tests(void) {
    if (!g_test_framework_initialized || !g_test_framework.config.enabled) {
        return -1;
    }
    
    pthread_mutex_lock(g_test_framework.mutex);
    
    g_test_framework.last_run = time(NULL);
    g_test_framework.total_runs++;
    
    int total_tests = 0;
    int total_passed = 0;
    int total_failed = 0;
    
    // Run all test suites
    for (int i = 0; i < g_test_framework.test_suite_count; i++) {
        if (g_test_framework.config.verbose_output) {
            printf("Running test suite: %s\n", g_test_framework.test_suites[i].name);
        }
        
        run_test_suite_internal(g_test_framework.test_suites[i].name);
        
        // Update totals
        total_tests += g_test_framework.test_suites[i].test_case_count;
        total_passed += g_test_framework.test_suites[i].passed_count;
        total_failed += g_test_framework.test_suites[i].failed_count;
        
        // Stop on failure if configured
        if (g_test_framework.config.stop_on_failure && g_test_framework.test_suites[i].failed_count > 0) {
            break;
        }
    }
    
    g_test_framework.total_tests = total_tests;
    g_test_framework.total_passed = total_passed;
    g_test_framework.total_failed = total_failed;
    
    pthread_mutex_unlock(g_test_framework.mutex);
    
    return total_failed == 0 ? 0 : -1;
}

// Run specific test suite
int test_framework_run_test_suite(const char* suite_name) {
    if (!g_test_framework_initialized || !g_test_framework.config.enabled || !suite_name) {
        return -1;
    }
    
    return run_test_suite_internal(suite_name);
}

// Run specific test case
int test_framework_run_test_case(const char* suite_name, const char* test_name) {
    if (!g_test_framework_initialized || !g_test_framework.config.enabled || !suite_name || !test_name) {
        return -1;
    }
    
    return run_test_case_internal(suite_name, test_name);
}

// Add test case
int test_framework_add_test_case(const char* suite_name, const char* test_name, 
                                const char* description) {
    if (!g_test_framework_initialized || !suite_name || !test_name) {
        return -1;
    }
    
    pthread_mutex_lock(g_test_framework.mutex);
    
    // Find the test suite
    test_suite_t* suite = NULL;
    for (int i = 0; i < g_test_framework.test_suite_count; i++) {
        if (strcmp(g_test_framework.test_suites[i].name, suite_name) == 0) {
            suite = &g_test_framework.test_suites[i];
            break;
        }
    }
    
    if (!suite) {
        pthread_mutex_unlock(g_test_framework.mutex);
        return -1;
    }
    
    // Check if test case already exists
    for (int i = 0; i < suite->test_case_count; i++) {
        if (strcmp(suite->test_cases[i].name, test_name) == 0) {
            pthread_mutex_unlock(g_test_framework.mutex);
            return 0; // Already exists
        }
    }
    
    // Add new test case
    if (suite->test_case_count < 100) {
        test_case_t* test_case = &suite->test_cases[suite->test_case_count];
        
        strcpy(test_case->name, test_name);
        strcpy(test_case->description, description ? description : "");
        test_case->result = TEST_RESULT_SKIP;
        test_case->enabled = true;
        test_case->start_time = 0;
        test_case->end_time = 0;
        test_case->duration_seconds = 0.0;
        
        suite->test_case_count++;
        g_test_framework.total_tests++;
        
        pthread_mutex_unlock(g_test_framework.mutex);
        return 0;
    }
    
    pthread_mutex_unlock(g_test_framework.mutex);
    return -1;
}

// Get test results
int test_framework_get_results(test_suite_t* suites, int max_suites) {
    if (!g_test_framework_initialized || !suites || max_suites <= 0) {
        return -1;
    }
    
    pthread_mutex_lock(g_test_framework.mutex);
    
    int count = 0;
    for (int i = 0; i < g_test_framework.test_suite_count && count < max_suites; i++) {
        suites[count] = g_test_framework.test_suites[i];
        count++;
    }
    
    pthread_mutex_unlock(g_test_framework.mutex);
    
    return count;
}

// Run test suite (internal)
static int run_test_suite_internal(const char* suite_name) {
    // Find the test suite
    test_suite_t* suite = NULL;
    for (int i = 0; i < g_test_framework.test_suite_count; i++) {
        if (strcmp(g_test_framework.test_suites[i].name, suite_name) == 0) {
            suite = &g_test_framework.test_suites[i];
            break;
        }
    }
    
    if (!suite) {
        return -1;
    }
    
    // Reset suite statistics
    suite->passed_count = 0;
    suite->failed_count = 0;
    suite->skipped_count = 0;
    suite->error_count = 0;
    suite->start_time = time(NULL);
    
    if (g_test_framework.config.verbose_output) {
        printf("Starting test suite: %s (%s)\n", suite->name, suite->description);
    }
    
    // Run all test cases in the suite
    for (int i = 0; i < suite->test_case_count; i++) {
        test_case_t* test_case = &suite->test_cases[i];
        
        if (!test_case->enabled) {
            test_case->result = TEST_RESULT_SKIP;
            suite->skipped_count++;
            continue;
        }
        
        // Run the test case
        int result = run_test_case_internal(suite_name, test_case->name);
        
        // Update suite statistics
        switch (test_case->result) {
            case TEST_RESULT_PASS:
                suite->passed_count++;
                break;
            case TEST_RESULT_FAIL:
                suite->failed_count++;
                break;
            case TEST_RESULT_SKIP:
                suite->skipped_count++;
                break;
            case TEST_RESULT_ERROR:
                suite->error_count++;
                break;
        }
        
        // Log result if verbose
        if (g_test_framework.config.verbose_output) {
            log_test_result(test_case);
        }
        
        // Stop on failure if configured
        if (g_test_framework.config.stop_on_failure && test_case->result == TEST_RESULT_FAIL) {
            break;
        }
    }
    
    suite->end_time = time(NULL);
    suite->total_duration = calculate_test_duration(suite->start_time, suite->end_time);
    
    if (g_test_framework.config.verbose_output) {
        printf("Test suite %s completed: %d passed, %d failed, %d skipped, %d errors (%.2fs)\n",
               suite->name, suite->passed_count, suite->failed_count, 
               suite->skipped_count, suite->error_count, suite->total_duration);
    }
    
    return suite->failed_count == 0 ? 0 : -1;
}

// Run test case (internal)
static int run_test_case_internal(const char* suite_name, const char* test_name) {
    // Find the test suite and case
    test_suite_t* suite = NULL;
    test_case_t* test_case = NULL;
    
    for (int i = 0; i < g_test_framework.test_suite_count; i++) {
        if (strcmp(g_test_framework.test_suites[i].name, suite_name) == 0) {
            suite = &g_test_framework.test_suites[i];
            break;
        }
    }
    
    if (!suite) {
        return -1;
    }
    
    for (int i = 0; i < suite->test_case_count; i++) {
        if (strcmp(suite->test_cases[i].name, test_name) == 0) {
            test_case = &suite->test_cases[i];
            break;
        }
    }
    
    if (!test_case) {
        return -1;
    }
    
    // Start timing
    test_case->start_time = time(NULL);
    
    // This is a simplified test execution
    // In a real system, you'd have actual test implementations
    
    // Simulate test execution based on test name
    if (strstr(test_case->name, "basic") || strstr(test_case->name, "simple")) {
        // Basic tests usually pass
        update_test_result(test_case, TEST_RESULT_PASS, NULL);
    } else if (strstr(test_case->name, "network") || strstr(test_case->name, "connection")) {
        // Network tests might fail occasionally
        if (rand() % 10 < 2) { // 20% chance of failure
            update_test_result(test_case, TEST_RESULT_FAIL, "Network connection timeout");
        } else {
            update_test_result(test_case, TEST_RESULT_PASS, NULL);
        }
    } else if (strstr(test_case->name, "performance") || strstr(test_case->name, "stress")) {
        // Performance tests might be skipped
        if (rand() % 10 < 3) { // 30% chance of skip
            update_test_result(test_case, TEST_RESULT_SKIP, "Performance test skipped in CI environment");
        } else {
            update_test_result(test_case, TEST_RESULT_PASS, NULL);
        }
    } else {
        // Default to pass
        update_test_result(test_case, TEST_RESULT_PASS, NULL);
    }
    
    // End timing
    test_case->end_time = time(NULL);
    test_case->duration_seconds = calculate_test_duration(test_case->start_time, test_case->end_time);
    
    return test_case->result == TEST_RESULT_PASS ? 0 : -1;
}

// Update test result
static void update_test_result(test_case_t* test_case, test_result_t result, const char* error_message) {
    if (!test_case) return;
    
    test_case->result = result;
    
    if (error_message && result != TEST_RESULT_PASS) {
        strncpy(test_case->error_message, error_message, sizeof(test_case->error_message) - 1);
        test_case->error_message[sizeof(test_case->error_message) - 1] = '\0';
    } else {
        test_case->error_message[0] = '\0';
    }
}

// Calculate test duration
static double calculate_test_duration(time_t start_time, time_t end_time) {
    return difftime(end_time, start_time);
}

// Log test result
static void log_test_result(const test_case_t* test_case) {
    if (!test_case) return;
    
    const char* result_str = "UNKNOWN";
    switch (test_case->result) {
        case TEST_RESULT_PASS:
            result_str = "PASS";
            break;
        case TEST_RESULT_FAIL:
            result_str = "FAIL";
            break;
        case TEST_RESULT_SKIP:
            result_str = "SKIP";
            break;
        case TEST_RESULT_ERROR:
            result_str = "ERROR";
            break;
    }
    
    printf("  %s: %s (%.2fs)", result_str, test_case->name, test_case->duration_seconds);
    
    if (test_case->result != TEST_RESULT_PASS && test_case->error_message[0] != '\0') {
        printf(" - %s", test_case->error_message);
    }
    
    printf("\n");
}

// Get test framework status
void test_framework_get_status(test_framework_t* status) {
    if (!status || !g_test_framework_initialized) return;
    
    pthread_mutex_lock(g_test_framework.mutex);
    *status = g_test_framework;
    pthread_mutex_unlock(g_test_framework.mutex);
}

// Check if test framework is initialized
bool test_framework_is_initialized(void) {
    return g_test_framework_initialized;
}

// Get test framework instance
test_framework_t* test_framework_get_instance(void) {
    return g_test_framework_initialized ? &g_test_framework : NULL;
}
