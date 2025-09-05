#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

#include <stdbool.h>
#include <time.h>

// Test result
typedef enum {
    TEST_RESULT_PASS,
    TEST_RESULT_FAIL,
    TEST_RESULT_SKIP,
    TEST_RESULT_ERROR
} test_result_t;

// Test case
typedef struct {
    char name[128];
    char description[256];
    test_result_t result;
    char error_message[512];
    time_t start_time;
    time_t end_time;
    double duration_seconds;
    bool enabled;
} test_case_t;

// Test suite
typedef struct {
    char name[128];
    char description[256];
    test_case_t test_cases[100];
    int test_case_count;
    int passed_count;
    int failed_count;
    int skipped_count;
    int error_count;
    time_t start_time;
    time_t end_time;
    double total_duration;
} test_suite_t;

// Test framework configuration
typedef struct {
    bool enabled;
    bool run_on_startup;
    bool stop_on_failure;
    bool verbose_output;
    char output_file[256];
    int timeout_seconds;
} test_framework_config_t;

// Test framework structure
typedef struct {
    test_framework_config_t config;
    
    // Test suites
    test_suite_t test_suites[10];
    int test_suite_count;
    
    // Current test
    test_suite_t* current_suite;
    test_case_t* current_test;
    
    // Statistics
    time_t last_run;
    int total_runs;
    int total_tests;
    int total_passed;
    int total_failed;
    
    // Mutex for thread safety
    pthread_mutex_t* mutex;
} test_framework_t;

// Initialize test framework
int test_framework_init(const test_framework_config_t* config);

// Clean up test framework
void test_framework_cleanup(void);

// Run all tests
int test_framework_run_all_tests(void);

// Run specific test suite
int test_framework_run_test_suite(const char* suite_name);

// Run specific test case
int test_framework_run_test_case(const char* suite_name, const char* test_name);

// Add test case
int test_framework_add_test_case(const char* suite_name, const char* test_name, 
                                const char* description);

// Get test results
int test_framework_get_results(test_suite_t* suites, int max_suites);

// Get test framework status
void test_framework_get_status(test_framework_t* status);

// Check if test framework is initialized
bool test_framework_is_initialized(void);

// Get test framework instance
test_framework_t* test_framework_get_instance(void);

#endif // TEST_FRAMEWORK_H
