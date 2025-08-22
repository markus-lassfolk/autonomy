package utils

import (
	"crypto/rand"
	"fmt"
	"os"
	"path/filepath"
	"time"
)

// SecureTempFile creates a secure temporary file with proper permissions
func SecureTempFile(dir, pattern string) (*os.File, error) {
	// Use system temp directory if none specified
	if dir == "" {
		dir = os.TempDir()
	}

	// Create a random suffix to prevent predictable names
	randomBytes := make([]byte, 8)
	if _, err := rand.Read(randomBytes); err != nil {
		return nil, fmt.Errorf("failed to generate random bytes: %w", err)
	}
	randomSuffix := fmt.Sprintf("%x", randomBytes)

	// Create secure filename
	filename := fmt.Sprintf("%s_%s_%s", pattern, time.Now().Format("20060102_150405"), randomSuffix)
	filepath := filepath.Join(dir, filename)

	// Create file with secure permissions (0600 = owner read/write only)
	file, err := os.OpenFile(filepath, os.O_CREATE|os.O_EXCL|os.O_WRONLY, 0o600)
	if err != nil {
		return nil, fmt.Errorf("failed to create secure temp file: %w", err)
	}

	return file, nil
}

// SecureTempDir creates a secure temporary directory
func SecureTempDir(dir, pattern string) (string, error) {
	// Use system temp directory if none specified
	if dir == "" {
		dir = os.TempDir()
	}

	// Create a random suffix to prevent predictable names
	randomBytes := make([]byte, 8)
	if _, err := rand.Read(randomBytes); err != nil {
		return "", fmt.Errorf("failed to generate random bytes: %w", err)
	}
	randomSuffix := fmt.Sprintf("%x", randomBytes)

	// Create secure directory name
	dirname := fmt.Sprintf("%s_%s_%s", pattern, time.Now().Format("20060102_150405"), randomSuffix)
	dirpath := filepath.Join(dir, dirname)

	// Create directory with secure permissions (0700 = owner read/write/execute only)
	if err := os.MkdirAll(dirpath, 0o700); err != nil {
		return "", fmt.Errorf("failed to create secure temp directory: %w", err)
	}

	return dirpath, nil
}

// CleanupTempFile safely removes a temporary file
func CleanupTempFile(filepath string) error {
	if filepath == "" {
		return nil
	}

	// Only remove files in temp directories for safety
	if !isInTempDir(filepath) {
		return fmt.Errorf("refusing to remove file outside temp directory: %s", filepath)
	}

	if err := os.Remove(filepath); err != nil && !os.IsNotExist(err) {
		return fmt.Errorf("failed to remove temp file: %w", err)
	}

	return nil
}

// CleanupTempDir safely removes a temporary directory and its contents
func CleanupTempDir(dirpath string) error {
	if dirpath == "" {
		return nil
	}

	// Only remove directories in temp directories for safety
	if !isInTempDir(dirpath) {
		return fmt.Errorf("refusing to remove directory outside temp directory: %s", dirpath)
	}

	if err := os.RemoveAll(dirpath); err != nil {
		return fmt.Errorf("failed to remove temp directory: %w", err)
	}

	return nil
}

// isInTempDir checks if a path is within a temporary directory
func isInTempDir(path string) bool {
	tempDir := os.TempDir()
	absPath, err := filepath.Abs(path)
	if err != nil {
		return false
	}
	absTempDir, err := filepath.Abs(tempDir)
	if err != nil {
		return false
	}
	return filepath.HasPrefix(absPath, absTempDir)
}

// GetSecureTempPath returns a secure temporary file path without creating the file
func GetSecureTempPath(dir, pattern string) (string, error) {
	// Use system temp directory if none specified
	if dir == "" {
		dir = os.TempDir()
	}

	// Create a random suffix to prevent predictable names
	randomBytes := make([]byte, 8)
	if _, err := rand.Read(randomBytes); err != nil {
		return "", fmt.Errorf("failed to generate random bytes: %w", err)
	}
	randomSuffix := fmt.Sprintf("%x", randomBytes)

	// Create secure filename
	filename := fmt.Sprintf("%s_%s_%s", pattern, time.Now().Format("20060102_150405"), randomSuffix)
	return filepath.Join(dir, filename), nil
}
