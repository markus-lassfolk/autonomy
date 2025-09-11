#include "src/c/autonomy-daemon/ml/ml_monitor_analytics.h"
#include <stdio.h>

int main() {
    printf("Size of ml_analytics_data_t: %zu bytes (%.2f MB)\n", 
           sizeof(ml_analytics_data_t), 
           sizeof(ml_analytics_data_t) / (1024.0 * 1024.0));
    return 0;
}
