#!/bin/bash

# Fix remaining shell injection vulnerabilities in workflow files

# Fix performance-monitoring.yml
sed -i "s/\${{ github\.event\.inputs\.benchmark_type || 
\all\' }}/\$BENCHMARK_TYPE_INPUT/g" .github/workflows/performance-monitoring.yml
sed -i "s/\${{ github\.event_name }}/\$EVENT_NAME_INPUT/g" .github/workflows/performance-monitoring.yml

# Add env section to performance-monitoring.yml
sed -i "/- name: Generate performance report/a \      env:\n        BENCHMARK_TYPE_INPUT: \${{ github.event.inputs.benchmark_type || \all\' }}\n        EVENT_NAME_INPUT: \${{ github.event_name }}" .github/workflows/performance-monitoring.yml

# Fix rutos-test-environment.yml
sed -i "s/\${{ github\.event\.inputs\.test_type || \full\' }}/\$TEST_TYPE_INPUT/g" .github/workflows/rutos-test-environment.yml
sed -i "s/\${{ matrix\.platform }}/\$PLATFORM_INPUT/g" .github/workflows/rutos-test-environment.yml

# Add env section to rutos-test-environment.yml
sed -i "/- name: Create test report/a \      env:\n        TEST_TYPE_INPUT: \${{ github.event.inputs.test_type || \full\' }}\n        PLATFORM_INPUT: \${{ matrix.platform }}" .github/workflows/rutos-test-environment.yml

# Fix webhook-receiver.yml
sed -i "s/\${{ github\.event\.inputs\.test_payload }}/\$TEST_PAYLOAD_INPUT/g" .github/workflows/webhook-receiver.yml
sed -i "s/\${{ github\.event_name }}/\$EVENT_NAME_INPUT/g" .github/workflows/webhook-receiver.yml

# Add env section to webhook-receiver.yml
sed -i "/- name: Get payload/a \      env:\n        TEST_PAYLOAD_INPUT: \${{ github.event.inputs.test_payload }}\n        EVENT_NAME_INPUT: \${{ github.event_name }}" .github/workflows/webhook-receiver.yml

echo "Shell injection fixes completed"
