#!/bin/bash

# Simple test package to verify ar archive creation

set -e

echo "=== Creating Simple Test Package ==="

# Create test directory
TEST_DIR="/tmp/simple-test"
rm -rf "$TEST_DIR"
mkdir -p "$TEST_DIR"

# Create control file
cat > "$TEST_DIR/control" << 'EOF'
Package: test-simple
Version: 1.0-1
Depends: libc
Section: test
Architecture: arm_cortex-a7_neon-vfpv4
Installed-Size: 100
Description: Simple test package
EOF

# Create postinst
cat > "$TEST_DIR/postinst" << 'EOF'
#!/bin/sh
echo "Test package installed"
exit 0
EOF

chmod +x "$TEST_DIR/postinst"

# Create data directory
mkdir -p "$TEST_DIR/data/usr/bin"
echo "#!/bin/sh" > "$TEST_DIR/data/usr/bin/test-simple"
echo "echo 'Hello from test package'" >> "$TEST_DIR/data/usr/bin/test-simple"
chmod +x "$TEST_DIR/data/usr/bin/test-simple"

# Create debian-binary
echo "2.0" > "$TEST_DIR/debian-binary"

# Create control.tar.gz
cd "$TEST_DIR"
tar -czf control.tar.gz --owner=root --group=root control postinst

# Create data.tar.gz
tar -czf data.tar.gz --owner=root --group=root -C data .

# Create IPK file
ar cr "test-simple_1.0-1_arm_cortex-a7_neon-vfpv4.ipk" debian-binary control.tar.gz data.tar.gz

# Compress the IPK file with gzip
gzip -c "test-simple_1.0-1_arm_cortex-a7_neon-vfpv4.ipk" > "test-simple_1.0-1_arm_cortex-a7_neon-vfpv4.ipk.gz"

# Copy to current directory
cp "test-simple_1.0-1_arm_cortex-a7_neon-vfpv4.ipk.gz" /mnt/j/GithubCursor/autonomy/

echo "✅ Test package created: test-simple_1.0-1_arm_cortex-a7_neon-vfpv4.ipk.gz"

# Verify package
echo "Verifying package structure..."
ar t "test-simple_1.0-1_arm_cortex-a7_neon-vfpv4.ipk"

echo "✅ Test package verification complete"
