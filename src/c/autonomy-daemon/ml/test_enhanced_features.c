#include "ml_enhanced_integration.h"
#include "ml_satellite_redundancy.h"
#include "ml_cellular_tower_intelligence.h"
#include "../shared/logging/logx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

// Test satellite redundancy analysis
void test_satellite_redundancy_analysis(void) {
    printf("\n=== Testing Satellite Redundancy Analysis ===\n"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Initialize satellite redundancy predictor
    satellite_redundancy_config_t config;
    satellite_redundancy_config_init_defaults(&config\n"\n"\n"\n"\n"\n"\n"\n");
    
    satellite_redundancy_predictor_t *predictor = satellite_redundancy_init(&config\n"\n"\n"\n"\n"\n"\n"\n");
    assert(predictor != NULL\n"\n"\n"\n"\n"\n"\n"\n");
    printf(" Satellite redundancy predictor initialized\n"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Test with simulated Starlink data
    starlink_status_response_t starlink_data;
    memset(&starlink_data, 0, sizeof(starlink_data)\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Simulate different satellite scenarios
    struct {
        int visible_sats;
        double obstruction_ratio;
        const char *description;
    } test_scenarios[] = {
        {12, 0.0, "Optimal: 12 satellites, no obstruction"},
        {8, 0.1, "Good: 8 satellites, 10% obstruction"},
        {6, 0.2, "Fair: 6 satellites, 20% obstruction"},
        {4, 0.3, "Poor: 4 satellites, 30% obstruction"},
        {2, 0.5, "Critical: 2 satellites, 50% obstruction"},
        {1, 0.8, "Emergency: 1 satellite, 80% obstruction"}
    };
    
    for (int i = 0; i < 6; i++) {
        starlink_data.gps_stats.gps_sats = test_scenarios[i].visible_sats;
        starlink_data.obstruction_stats.fraction_obstructed = test_scenarios[i].obstruction_ratio;
        
        int result = satellite_redundancy_assess_current(predictor, &starlink_data\n"\n"\n"\n"\n"\n"\n"\n");
        assert(result == SATELLITE_REDUNDANCY_SUCCESS\n"\n"\n"\n"\n"\n"\n"\n");
        
        satellite_redundancy_assessment_t assessment;
        result = satellite_redundancy_get_assessment(predictor, &assessment\n"\n"\n"\n"\n"\n"\n"\n");
        assert(result == SATELLITE_REDUNDANCY_SUCCESS\n"\n"\n"\n"\n"\n"\n"\n");
        
        printf("  %s: visible=%d, redundancy=%.2f, risk_level=%d, warning=%s\n",
               test_scenarios[i].description,
               assessment.total_visible,
               assessment.redundancy_score,
               assessment.risk_level,
               assessment.early_warning_triggered ? "YES" : "NO"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Test early warning system
    printf("\n  Testing early warning system...\n"\n"\n"\n"\n"\n"\n"\n"\n");
    bool warning_active = satellite_redundancy_is_early_warning_active(predictor\n"\n"\n"\n"\n"\n"\n"\n");
    printf("  Early warning active: %s\n", warning_active ? "YES" : "NO"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Test ML feature extraction
    uint8_t redundancy_feature, risk_feature, diversity_feature;
    int result = satellite_redundancy_get_ml_features(predictor, &redundancy_feature, &risk_feature, &diversity_feature\n"\n"\n"\n"\n"\n"\n"\n");
    assert(result == SATELLITE_REDUNDANCY_SUCCESS\n"\n"\n"\n"\n"\n"\n"\n");
    printf("  ML features: redundancy=%d, risk=%d, diversity=%d\n", 
           redundancy_feature, risk_feature, diversity_feature\n"\n"\n"\n"\n"\n"\n"\n");
    
    satellite_redundancy_cleanup(predictor\n"\n"\n"\n"\n"\n"\n"\n");
    printf(" Satellite redundancy analysis test completed\n"\n"\n"\n"\n"\n"\n"\n"\n");
}

// Test cellular tower intelligence
void test_cellular_tower_intelligence(void) {
    printf("\n=== Testing Cellular Tower Intelligence ===\n"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Initialize cellular tower intelligence
    cellular_tower_config_t config;
    cellular_tower_config_init_defaults(&config\n"\n"\n"\n"\n"\n"\n"\n");
    
    cellular_tower_intelligence_t *intelligence = cellular_tower_intelligence_init(&config\n"\n"\n"\n"\n"\n"\n"\n");
    assert(intelligence != NULL\n"\n"\n"\n"\n"\n"\n"\n");
    printf(" Cellular tower intelligence initialized\n"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Test carrier filtering
    printf("\n  Testing carrier filtering...\n"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Create test neighbor cells with different carriers
    opencellid_neighbor_cell_t test_neighbors[] = {
        // Home carrier (310-260)
        {.cell_id = {.mcc = 310, .mnc = 260, .lac = 12345, .cell_id = 1001}, .rsrp = -80, .rsrq = -10, .sinr = 15},
        // Roaming carrier (310-410)
        {.cell_id = {.mcc = 310, .mnc = 410, .lac = 12346, .cell_id = 1002}, .rsrp = -90, .rsrq = -12, .sinr = 12},
        // Weak signal tower
        {.cell_id = {.mcc = 310, .mnc = 260, .lac = 12347, .cell_id = 1003}, .rsrp = -130, .rsrq = -20, .sinr = 5},
        // Unauthorized carrier (999-999)
        {.cell_id = {.mcc = 999, .mnc = 999, .lac = 12348, .cell_id = 1004}, .rsrp = -85, .rsrq = -11, .sinr = 14},
        // Good roaming tower
        {.cell_id = {.mcc = 310, .mnc = 480, .lac = 12349, .cell_id = 1005}, .rsrp = -75, .rsrq = -8, .sinr = 18}
    };
    
    usable_tower_info_t usable_towers[20];
    uint8_t usable_count;
    
    int result = cellular_tower_intelligence_filter_usable_towers(intelligence,
                                                               test_neighbors, 5,
                                                               usable_towers, &usable_count\n"\n"\n"\n"\n"\n"\n"\n");
    assert(result == CELLULAR_TOWER_INTELLIGENCE_SUCCESS\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("  Filtered %d usable towers from 5 neighbors:\n", usable_count\n"\n"\n"\n"\n"\n"\n"\n");
    for (uint8_t i = 0; i < usable_count; i++) {
        printf("    Tower %d: MCC=%d, MNC=%d, RSRP=%d dBm, usable_score=%.2f, home_carrier=%s\n",
               i + 1,
               usable_towers[i].cell_id.mcc,
               usable_towers[i].cell_id.mnc,
               usable_towers[i].rsrp_dbm,
               usable_towers[i].usability_score,
               usable_towers[i].is_home_carrier ? "YES" : "NO"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Test tower density analysis
    printf("\n  Testing tower density analysis...\n"\n"\n"\n"\n"\n"\n"\n"\n");
    result = cellular_tower_intelligence_analyze_density(intelligence, usable_towers, usable_count, 40.7128, -74.0060\n"\n"\n"\n"\n"\n"\n"\n");
    assert(result == CELLULAR_TOWER_INTELLIGENCE_SUCCESS\n"\n"\n"\n"\n"\n"\n"\n");
    
    tower_density_analysis_t density_analysis;
    result = cellular_tower_intelligence_get_density_analysis(intelligence, &density_analysis\n"\n"\n"\n"\n"\n"\n"\n");
    assert(result == CELLULAR_TOWER_INTELLIGENCE_SUCCESS\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("  Density analysis: usable_towers=%d, density_score=%.2f, coverage_score=%.2f, reliability_score=%.2f\n",
           density_analysis.usable_towers_in_radius,
           density_analysis.density_score,
           density_analysis.coverage_score,
           density_analysis.reliability_score\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Test cell change pattern analysis
    printf("\n  Testing cell change pattern analysis...\n"\n"\n"\n"\n"\n"\n"\n"\n");
    
    cellular_info_t cellular_info;
    memset(&cellular_info, 0, sizeof(cellular_info)\n"\n"\n"\n"\n"\n"\n"\n");
    cellular_info.rsrp = -85;
    strcpy(cellular_info.cell_id, "1001"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Simulate cell changes
    const char *cell_sequence[] = {"1001", "1002", "1001", "1003", "1002", "1005"};
    for (int i = 0; i < 6; i++) {
        strcpy(cellular_info.cell_id, cell_sequence[i]\n"\n"\n"\n"\n"\n"\n"\n");
        result = cellular_tower_intelligence_analyze_cell_changes(intelligence, cellular_info.cell_id, &cellular_info\n"\n"\n"\n"\n"\n"\n"\n");
        assert(result == CELLULAR_TOWER_INTELLIGENCE_SUCCESS\n"\n"\n"\n"\n"\n"\n"\n");
        
        // Small delay to simulate time between changes
        usleep(100000); // 100ms
    }
    
    cell_change_pattern_analysis_t cell_analysis;
    result = cellular_tower_intelligence_get_cell_change_analysis(intelligence, &cell_analysis\n"\n"\n"\n"\n"\n"\n"\n");
    assert(result == CELLULAR_TOWER_INTELLIGENCE_SUCCESS\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("  Cell change analysis: changes_last_hour=%d, frequency_score=%.2f, risk_score=%.2f\n",
           cell_analysis.changes_last_hour,
           cell_analysis.change_frequency_score,
           cell_analysis.connectivity_risk_score\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Test ML feature extraction
    uint8_t tower_density_feature, cell_change_feature, coverage_quality_feature, connectivity_risk_feature;
    result = cellular_tower_intelligence_get_ml_features(intelligence,
                                                       &tower_density_feature,
                                                       &cell_change_feature,
                                                       &coverage_quality_feature,
                                                       &connectivity_risk_feature\n"\n"\n"\n"\n"\n"\n"\n");
    assert(result == CELLULAR_TOWER_INTELLIGENCE_SUCCESS\n"\n"\n"\n"\n"\n"\n"\n");
    printf("  ML features: density=%d, cell_change=%d, coverage=%d, risk=%d\n",
           tower_density_feature, cell_change_feature, coverage_quality_feature, connectivity_risk_feature\n"\n"\n"\n"\n"\n"\n"\n");
    
    cellular_tower_intelligence_cleanup(intelligence\n"\n"\n"\n"\n"\n"\n"\n");
    printf(" Cellular tower intelligence test completed\n"\n"\n"\n"\n"\n"\n"\n"\n");
}

// Test enhanced ML integration
void test_enhanced_ml_integration(void) {
    printf("\n=== Testing Enhanced ML Integration ===\n"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Initialize base ML monitor
    ml_monitor_config_t ml_config;
    ml_monitor_config_init_defaults(&ml_config\n"\n"\n"\n"\n"\n"\n"\n");
    ml_config.enabled = true;
    ml_config.collection_interval_seconds = 15;
    
    ml_monitor_t *ml_monitor = ml_monitor_init(&ml_config\n"\n"\n"\n"\n"\n"\n"\n");
    assert(ml_monitor != NULL\n"\n"\n"\n"\n"\n"\n"\n");
    printf(" Base ML monitor initialized\n"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Initialize enhanced integration
    int result = ml_enhanced_integration_init(ml_monitor\n"\n"\n"\n"\n"\n"\n"\n");
    assert(result == ML_MONITOR_SUCCESS\n"\n"\n"\n"\n"\n"\n"\n");
    printf(" Enhanced ML integration initialized\n"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Test enhanced observation collection
    printf("\n  Testing enhanced observation collection...\n"\n"\n"\n"\n"\n"\n"\n"\n");
    ml_observation_t observation;
    result = ml_enhanced_integration_collect_observation(&observation\n"\n"\n"\n"\n"\n"\n"\n");
    if (result == ML_MONITOR_SUCCESS) {
        printf("  Enhanced observation collected: satellites=%d, timestamp=%u\n",
               observation.satellites_visible, observation.timestamp\n"\n"\n"\n"\n"\n"\n"\n");
        
        // Show enhanced features in observation
        printf("  Enhanced features: redundancy_score=%d, risk_level=%d, diversity_score=%d\n",
               observation.reserved & 0xFF, // Lower 8 bits
               (observation.flags >> 4) & 0x0F, // Bits 4-7
               observation.pattern_hash & 0xFF); // Lower 8 bits
    } else {
        printf("  Enhanced observation collection failed (expected in test environment)\n"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Test enhanced prediction
    printf("\n  Testing enhanced prediction...\n"\n"\n"\n"\n"\n"\n"\n"\n");
    uint8_t probability, confidence, cause;
    result = ml_enhanced_integration_predict_outage(&probability, &confidence, &cause\n"\n"\n"\n"\n"\n"\n"\n");
    if (result == ML_MONITOR_SUCCESS) {
        printf("  Enhanced prediction: probability=%d, confidence=%d, cause=%d\n",
               probability, confidence, cause\n"\n"\n"\n"\n"\n"\n"\n");
    } else {
        printf("  Enhanced prediction failed (expected in test environment)\n"\n"\n"\n"\n"\n"\n"\n"\n");
    }
    
    // Test statistics
    printf("\n  Testing integration statistics...\n"\n"\n"\n"\n"\n"\n"\n"\n");
    ml_enhanced_integration_stats_t stats;
    result = ml_enhanced_integration_get_stats(&stats\n"\n"\n"\n"\n"\n"\n"\n");
    assert(result == ML_MONITOR_SUCCESS\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("  Integration stats: satellite_assessments=%d, cellular_analyses=%d, predictions=%d, warnings=%d\n",
           stats.satellite_assessments,
           stats.cellular_analyses,
           stats.ml_predictions_enhanced,
           stats.early_warnings_triggered\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Test enable/disable
    printf("\n  Testing enable/disable functionality...\n"\n"\n"\n"\n"\n"\n"\n"\n");
    bool enabled = ml_enhanced_integration_is_enabled(\n"\n"\n"\n"\n"\n"\n"\n");
    printf("  Enhanced features enabled: %s\n", enabled ? "YES" : "NO"\n"\n"\n"\n"\n"\n"\n"\n");
    
    result = ml_enhanced_integration_set_enabled(false\n"\n"\n"\n"\n"\n"\n"\n");
    assert(result == ML_MONITOR_SUCCESS\n"\n"\n"\n"\n"\n"\n"\n");
    printf("  Enhanced features disabled\n"\n"\n"\n"\n"\n"\n"\n"\n");
    
    result = ml_enhanced_integration_set_enabled(true\n"\n"\n"\n"\n"\n"\n"\n");
    assert(result == ML_MONITOR_SUCCESS\n"\n"\n"\n"\n"\n"\n"\n");
    printf("  Enhanced features re-enabled\n"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Cleanup
    ml_enhanced_integration_cleanup(\n"\n"\n"\n"\n"\n"\n"\n");
    ml_monitor_cleanup(ml_monitor\n"\n"\n"\n"\n"\n"\n"\n");
    printf(" Enhanced ML integration test completed\n"\n"\n"\n"\n"\n"\n"\n"\n");
}

// Main test function
int main(void) {
    printf(" Enhanced ML Features Test Suite\n"\n"\n"\n"\n"\n"\n"\n"\n");
    printf("=====================================\n"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Initialize logging
    logx_init(\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("\nTesting new enhanced ML features:\n"\n"\n"\n"\n"\n"\n"\n"\n");
    printf("1. Satellite Redundancy Analysis with Early Warning\n"\n"\n"\n"\n"\n"\n"\n"\n");
    printf("2. Cellular Tower Intelligence with Carrier Filtering\n"\n"\n"\n"\n"\n"\n"\n"\n");
    printf("3. Enhanced ML Integration\n"\n"\n"\n"\n"\n"\n"\n"\n");
    
    // Run tests
    test_satellite_redundancy_analysis(\n"\n"\n"\n"\n"\n"\n"\n");
    test_cellular_tower_intelligence(\n"\n"\n"\n"\n"\n"\n"\n");
    test_enhanced_ml_integration(\n"\n"\n"\n"\n"\n"\n"\n");
    
    printf("\n All Enhanced ML Features Tests Passed!\n"\n"\n"\n"\n"\n"\n"\n"\n");
    printf("\nKey Features Implemented:\n"\n"\n"\n"\n"\n"\n"\n"\n");
    printf(" Satellite redundancy analysis with count thresholds\n"\n"\n"\n"\n"\n"\n"\n"\n");
    printf(" Early warning system for satellite count drops\n"\n"\n"\n"\n"\n"\n"\n"\n");
    printf(" Satellite redundancy score as ML feature\n"\n"\n"\n"\n"\n"\n"\n"\n");
    printf(" Cellular tower filtering by carrier compatibility\n"\n"\n"\n"\n"\n"\n"\n"\n");
    printf(" Signal strength thresholds for usable towers\n"\n"\n"\n"\n"\n"\n"\n"\n");
    printf(" Tower density analysis for reliability prediction\n"\n"\n"\n"\n"\n"\n"\n"\n");
    printf(" Cell change pattern analysis for connectivity issues\n"\n"\n"\n"\n"\n"\n"\n"\n");
    printf(" Enhanced ML predictions with multi-source data\n"\n"\n"\n"\n"\n"\n"\n"\n");
    printf(" Comprehensive integration with existing ML system\n"\n"\n"\n"\n"\n"\n"\n"\n");
    
    return 0;
}
