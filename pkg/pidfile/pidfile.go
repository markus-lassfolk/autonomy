package pidfile

import (
	"fmt"
	"os"
	"os/exec"
	"path/filepath"
	"strconv"
	"strings"
)

// PIDFile represents a PID file for daemon process management
type PIDFile struct {
	path string
	pid  int
}

// New creates a new PIDFile instance
func New(path string) *PIDFile {
	return &PIDFile{
		path: path,
		pid:  os.Getpid(),
	}
}

// Create creates the PID file and locks it to prevent multiple instances
func (p *PIDFile) Create() error {
	// Check if PID file already exists
	if p.exists() {
		existingPID, err := p.readExistingPID()
		if err != nil {
			return fmt.Errorf("failed to read existing PID file: %w", err)
		}

		// Check if the process is still running
		if p.isProcessRunning(existingPID) {
			return fmt.Errorf("daemon already running with PID %d", existingPID)
		}

		// Process is not running, remove stale PID file
		if err := os.Remove(p.path); err != nil {
			return fmt.Errorf("failed to remove stale PID file: %w", err)
		}
	}

	// Create directory if it doesn't exist
	dir := filepath.Dir(p.path)
	if err := os.MkdirAll(dir, 0o755); err != nil {
		return fmt.Errorf("failed to create PID file directory: %w", err)
	}

	// Create and write PID file
	if err := os.WriteFile(p.path, []byte(fmt.Sprintf("%d\n", p.pid)), 0o644); err != nil {
		return fmt.Errorf("failed to create PID file: %w", err)
	}

	return nil
}

// Remove removes the PID file
func (p *PIDFile) Remove() error {
	if !p.exists() {
		return nil // Already removed
	}

	// Verify this is our PID file before removing
	existingPID, err := p.readExistingPID()
	if err != nil {
		// If we can't read it, try to remove it anyway
		return os.Remove(p.path)
	}

	if existingPID != p.pid {
		return fmt.Errorf("PID file contains different PID (%d vs %d), not removing", existingPID, p.pid)
	}

	return os.Remove(p.path)
}

// GetPID returns the PID stored in the file
func (p *PIDFile) GetPID() (int, error) {
	return p.readExistingPID()
}

// Path returns the path to the PID file
func (p *PIDFile) Path() string {
	return p.path
}

// exists checks if the PID file exists
func (p *PIDFile) exists() bool {
	_, err := os.Stat(p.path)
	return err == nil
}

// readExistingPID reads the PID from an existing PID file
func (p *PIDFile) readExistingPID() (int, error) {
	data, err := os.ReadFile(p.path)
	if err != nil {
		return 0, err
	}

	pidStr := strings.TrimSpace(string(data))
	pid, err := strconv.Atoi(pidStr)
	if err != nil {
		return 0, fmt.Errorf("invalid PID in file: %s", pidStr)
	}

	return pid, nil
}

// isProcessRunning checks if a process with the given PID is running
func (p *PIDFile) isProcessRunning(pid int) bool {
	// Use cross-platform approach to check if process exists
	// Try to find the process using system-specific methods
	return p.findProcess(pid)
}

// findProcess tries to find a process by PID using system-specific methods
func (p *PIDFile) findProcess(pid int) bool {
	// Try to use ps command if available (Unix-like systems)
	cmd := exec.Command("ps", "-p", strconv.Itoa(pid))
	if err := cmd.Run(); err == nil {
		return true // Process found
	}

	// Try BusyBox ps (no -p option, use grep instead)
	cmd = exec.Command("sh", "-c", "ps | grep '^"+strconv.Itoa(pid)+" '")
	if err := cmd.Run(); err == nil {
		return true // Process found
	}

	// Try tasklist on Windows
	cmd = exec.Command("tasklist", "/FI", "PID eq "+strconv.Itoa(pid))
	if err := cmd.Run(); err == nil {
		return true // Process found
	}

	// If we can't determine, assume process doesn't exist
	return false
}

// ForceRemove forcefully removes the PID file regardless of ownership
// This should only be used in cleanup scenarios
func (p *PIDFile) ForceRemove() error {
	return os.Remove(p.path)
}

// CheckRunning checks if another instance is running and returns its PID
func (p *PIDFile) CheckRunning() (bool, int, error) {
	if !p.exists() {
		return false, 0, nil
	}

	existingPID, err := p.readExistingPID()
	if err != nil {
		return false, 0, fmt.Errorf("failed to read PID file: %w", err)
	}

	if p.isProcessRunning(existingPID) {
		return true, existingPID, nil
	}

	return false, existingPID, nil
}
