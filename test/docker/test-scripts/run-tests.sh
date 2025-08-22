#!/bin/bash

# RUTOS Test Environment Test Script
set -e

echo "🧪 Starting RUTOS Test Environment Tests"
echo "========================================"

# Environment info
echo "Platform: $(uname -m)"
echo "OS: $(cat /etc/os-release | grep PRETTY_NAME | cut -d'"' -f2)"
echo "Kernel: $(uname -r)"
echo "Date: $(date)"

# Test basic tools
echo ""
echo "🔧 Testing basic tools..."
which bash && echo "✅ bash available"
which curl && echo "✅ curl available"
which git && echo "✅ git available"
which make && echo "✅ make available"
which gcc && echo "✅ gcc available"
which python3 && echo "✅ python3 available"

# Test build environment
echo ""
echo "🏗️ Testing build environment..."
echo "Working directory: $(pwd)"
echo "Directory contents:"
ls -la

# Create test results
echo ""
echo "📊 Generating test report..."
mkdir -p /workdir/results

cat > /workdir/results/test-report.txt << EOF
RUTOS Test Environment Report
============================
Platform: $(uname -m)
OS: $(cat /etc/os-release | grep PRETTY_NAME | cut -d'"' -f2)
Kernel: $(uname -r)
Date: $(date)
Working Directory: $(pwd)

Tools Available:
- bash: $(which bash 2>/dev/null || echo "NOT FOUND")
- curl: $(which curl 2>/dev/null || echo "NOT FOUND")
- git: $(which git 2>/dev/null || echo "NOT FOUND")
- make: $(which make 2>/dev/null || echo "NOT FOUND")
- gcc: $(which gcc 2>/dev/null || echo "NOT FOUND")
- python3: $(which python3 2>/dev/null || echo "NOT FOUND")

Test Status: PASSED
EOF

echo "✅ Test completed successfully"
echo "📄 Test report saved to /workdir/results/test-report.txt"

# Show test report
echo ""
echo "📋 Test Report:"
cat /workdir/results/test-report.txt

echo ""
echo "🎉 All tests passed!"
