package notifications

import (
	"fmt"
	"strings"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// NotificationDeduplicator prevents duplicate notifications using fingerprinting and similarity detection
type NotificationDeduplicator struct {
	config *SmartManagerConfig
	logger *logx.Logger

	// Deduplication tracking
	mu            sync.RWMutex
	fingerprints  map[string]time.Time     // fingerprint -> last seen time
	notifications map[string]*Notification // fingerprint -> notification for similarity comparison

	// Statistics
	totalChecked    int64
	duplicatesFound int64
	lastCleanup     time.Time
}

// NewNotificationDeduplicator creates a new notification deduplicator
func NewNotificationDeduplicator(config *SmartManagerConfig, logger *logx.Logger) *NotificationDeduplicator {
	nd := &NotificationDeduplicator{
		config:        config,
		logger:        logger,
		fingerprints:  make(map[string]time.Time),
		notifications: make(map[string]*Notification),
		lastCleanup:   time.Now(),
	}

	// Start cleanup routine
	go nd.cleanupLoop()

	return nd
}

// IsDuplicate checks if a notification is a duplicate based on fingerprint and similarity
func (nd *NotificationDeduplicator) IsDuplicate(notification *Notification, fingerprint string) bool {
	nd.mu.Lock()
	defer nd.mu.Unlock()

	nd.totalChecked++

	now := time.Now()

	// Check exact fingerprint match within window
	if lastSeen, exists := nd.fingerprints[fingerprint]; exists {
		if now.Sub(lastSeen) < nd.config.DeduplicationWindow {
			nd.duplicatesFound++
			nd.logger.Debug("Exact duplicate detected",
				"fingerprint", fingerprint[:8],
				"time_since_last", now.Sub(lastSeen))
			return true
		}
	}

	// Check similarity with existing notifications
	if nd.config.SimilarityThreshold > 0 {
		for existingFingerprint, existingNotification := range nd.notifications {
			if lastSeen, exists := nd.fingerprints[existingFingerprint]; exists {
				if now.Sub(lastSeen) < nd.config.DeduplicationWindow {
					similarity := nd.calculateSimilarity(notification, existingNotification)
					if similarity >= nd.config.SimilarityThreshold {
						nd.duplicatesFound++
						nd.logger.Debug("Similar duplicate detected",
							"fingerprint", fingerprint[:8],
							"existing_fingerprint", existingFingerprint[:8],
							"similarity", similarity)
						return true
					}
				}
			}
		}
	}

	// Not a duplicate - record it
	nd.fingerprints[fingerprint] = now
	nd.notifications[fingerprint] = notification

	return false
}

// calculateSimilarity calculates similarity between two notifications (0.0 to 1.0)
func (nd *NotificationDeduplicator) calculateSimilarity(n1, n2 *Notification) float64 {
	var similarities []float64

	// Type similarity (exact match or not)
	if n1.Type == n2.Type {
		similarities = append(similarities, 1.0)
	} else {
		similarities = append(similarities, 0.0)
	}

	// Priority similarity (closer priorities are more similar)
	priorityDiff := abs(n1.Priority - n2.Priority)
	prioritySimilarity := 1.0 - (float64(priorityDiff) / 4.0) // Max priority diff is 4 (-2 to 2)
	if prioritySimilarity < 0 {
		prioritySimilarity = 0
	}
	similarities = append(similarities, prioritySimilarity)

	// Title similarity (Levenshtein distance based)
	titleSimilarity := nd.calculateStringSimilarity(n1.Title, n2.Title)
	similarities = append(similarities, titleSimilarity)

	// Message similarity (Levenshtein distance based)
	messageSimilarity := nd.calculateStringSimilarity(n1.Message, n2.Message)
	similarities = append(similarities, messageSimilarity)

	// Context similarity (check key overlaps)
	contextSimilarity := nd.calculateContextSimilarity(n1.Context, n2.Context)
	similarities = append(similarities, contextSimilarity)

	// Calculate weighted average
	weights := []float64{0.3, 0.1, 0.3, 0.25, 0.05} // Type, Priority, Title, Message, Context

	totalWeight := 0.0
	weightedSum := 0.0

	for i, similarity := range similarities {
		if i < len(weights) {
			weightedSum += similarity * weights[i]
			totalWeight += weights[i]
		}
	}

	if totalWeight == 0 {
		return 0.0
	}

	return weightedSum / totalWeight
}

// calculateStringSimilarity calculates similarity between two strings using Levenshtein distance
func (nd *NotificationDeduplicator) calculateStringSimilarity(s1, s2 string) float64 {
	if s1 == s2 {
		return 1.0
	}

	if len(s1) == 0 || len(s2) == 0 {
		return 0.0
	}

	// Normalize strings (lowercase, trim)
	s1 = strings.ToLower(strings.TrimSpace(s1))
	s2 = strings.ToLower(strings.TrimSpace(s2))

	if s1 == s2 {
		return 1.0
	}

	// Calculate Levenshtein distance
	distance := nd.levenshteinDistance(s1, s2)
	maxLen := max(len(s1), len(s2))

	if maxLen == 0 {
		return 1.0
	}

	similarity := 1.0 - (float64(distance) / float64(maxLen))
	if similarity < 0 {
		similarity = 0
	}

	return similarity
}

// levenshteinDistance calculates the Levenshtein distance between two strings
func (nd *NotificationDeduplicator) levenshteinDistance(s1, s2 string) int {
	if len(s1) == 0 {
		return len(s2)
	}
	if len(s2) == 0 {
		return len(s1)
	}

	// Create matrix
	matrix := make([][]int, len(s1)+1)
	for i := range matrix {
		matrix[i] = make([]int, len(s2)+1)
	}

	// Initialize first row and column
	for i := 0; i <= len(s1); i++ {
		matrix[i][0] = i
	}
	for j := 0; j <= len(s2); j++ {
		matrix[0][j] = j
	}

	// Fill matrix
	for i := 1; i <= len(s1); i++ {
		for j := 1; j <= len(s2); j++ {
			cost := 0
			if s1[i-1] != s2[j-1] {
				cost = 1
			}

			matrix[i][j] = min(
				matrix[i-1][j]+1,      // deletion
				matrix[i][j-1]+1,      // insertion
				matrix[i-1][j-1]+cost, // substitution
			)
		}
	}

	return matrix[len(s1)][len(s2)]
}

// calculateContextSimilarity calculates similarity between context maps
func (nd *NotificationDeduplicator) calculateContextSimilarity(c1, c2 map[string]interface{}) float64 {
	if c1 == nil && c2 == nil {
		return 1.0
	}
	if c1 == nil || c2 == nil {
		return 0.0
	}

	// Get all unique keys
	allKeys := make(map[string]bool)
	for key := range c1 {
		allKeys[key] = true
	}
	for key := range c2 {
		allKeys[key] = true
	}

	if len(allKeys) == 0 {
		return 1.0
	}

	matchingKeys := 0
	for key := range allKeys {
		v1, exists1 := c1[key]
		v2, exists2 := c2[key]

		if exists1 && exists2 {
			// Both have the key, check if values are similar
			if fmt.Sprintf("%v", v1) == fmt.Sprintf("%v", v2) {
				matchingKeys++
			}
		}
		// If only one has the key, it's not a match
	}

	return float64(matchingKeys) / float64(len(allKeys))
}

// cleanupLoop periodically cleans up old fingerprints
func (nd *NotificationDeduplicator) cleanupLoop() {
	ticker := time.NewTicker(5 * time.Minute)
	defer ticker.Stop()

	for {
		select {
		case <-ticker.C:
			nd.cleanup()
		}
	}
}

// cleanup removes old fingerprints outside the deduplication window
func (nd *NotificationDeduplicator) cleanup() {
	nd.mu.Lock()
	defer nd.mu.Unlock()

	now := time.Now()
	cutoff := now.Add(-nd.config.DeduplicationWindow)

	removedCount := 0

	// Clean fingerprints
	for fingerprint, lastSeen := range nd.fingerprints {
		if lastSeen.Before(cutoff) {
			delete(nd.fingerprints, fingerprint)
			delete(nd.notifications, fingerprint)
			removedCount++
		}
	}

	if removedCount > 0 {
		nd.logger.Debug("Cleaned up old deduplication entries",
			"removed_count", removedCount,
			"remaining_count", len(nd.fingerprints))
	}

	nd.lastCleanup = now
}

// GetStats returns deduplication statistics
func (nd *NotificationDeduplicator) GetStats() map[string]interface{} {
	nd.mu.RLock()
	defer nd.mu.RUnlock()

	dedupeRate := float64(0)
	if nd.totalChecked > 0 {
		dedupeRate = float64(nd.duplicatesFound) / float64(nd.totalChecked)
	}

	return map[string]interface{}{
		"total_checked":        nd.totalChecked,
		"duplicates_found":     nd.duplicatesFound,
		"deduplication_rate":   dedupeRate,
		"active_fingerprints":  len(nd.fingerprints),
		"last_cleanup":         nd.lastCleanup,
		"window_duration":      nd.config.DeduplicationWindow.String(),
		"similarity_threshold": nd.config.SimilarityThreshold,
	}
}

// Clear clears all deduplication data (useful for testing)
func (nd *NotificationDeduplicator) Clear() {
	nd.mu.Lock()
	defer nd.mu.Unlock()

	nd.fingerprints = make(map[string]time.Time)
	nd.notifications = make(map[string]*Notification)
	nd.totalChecked = 0
	nd.duplicatesFound = 0

	nd.logger.Debug("Cleared all deduplication data")
}

// Helper functions
func abs(x int) int {
	if x < 0 {
		return -x
	}
	return x
}

func min(a, b, c int) int {
	if a < b {
		if a < c {
			return a
		}
		return c
	}
	if b < c {
		return b
	}
	return c
}

func max(a, b int) int {
	if a > b {
		return a
	}
	return b
}
