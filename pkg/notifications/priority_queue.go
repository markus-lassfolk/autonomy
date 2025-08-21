package notifications

import (
	"container/heap"
	"sync"
	"time"

	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// PriorityNotificationQueue manages notifications in priority order
type PriorityNotificationQueue struct {
	mu     sync.Mutex
	heap   *NotificationHeap
	logger *logx.Logger

	// Statistics
	totalEnqueued int64
	totalDequeued int64
	maxSize       int
	currentSize   int
}

// NotificationHeap implements heap.Interface for priority queue
type NotificationHeap []*QueuedNotification

// QueuedNotification wraps a notification with queue metadata
type QueuedNotification struct {
	Notification *Notification
	EnqueueTime  time.Time
	Priority     int   // Higher number = higher priority
	Sequence     int64 // For stable sorting
}

// NewPriorityNotificationQueue creates a new priority notification queue
func NewPriorityNotificationQueue(logger *logx.Logger) *PriorityNotificationQueue {
	pnq := &PriorityNotificationQueue{
		heap:   &NotificationHeap{},
		logger: logger,
	}

	heap.Init(pnq.heap)
	return pnq
}

// Enqueue adds a notification to the priority queue
func (pnq *PriorityNotificationQueue) Enqueue(notification *Notification) {
	pnq.mu.Lock()
	defer pnq.mu.Unlock()

	queuedNotif := &QueuedNotification{
		Notification: notification,
		EnqueueTime:  time.Now(),
		Priority:     pnq.calculateQueuePriority(notification.Priority),
		Sequence:     pnq.totalEnqueued,
	}

	heap.Push(pnq.heap, queuedNotif)
	pnq.totalEnqueued++
	pnq.currentSize++

	if pnq.currentSize > pnq.maxSize {
		pnq.maxSize = pnq.currentSize
	}

	pnq.logger.Debug("Notification enqueued",
		"type", notification.Type,
		"priority", notification.Priority,
		"queue_priority", queuedNotif.Priority,
		"queue_size", pnq.currentSize)
}

// Dequeue removes and returns the highest priority notification
func (pnq *PriorityNotificationQueue) Dequeue() *Notification {
	pnq.mu.Lock()
	defer pnq.mu.Unlock()

	if pnq.heap.Len() == 0 {
		return nil
	}

	queuedNotif := heap.Pop(pnq.heap).(*QueuedNotification)
	pnq.totalDequeued++
	pnq.currentSize--

	// Check if notification has expired (been in queue too long)
	maxAge := pnq.getMaxAge(queuedNotif.Notification.Priority)
	if time.Since(queuedNotif.EnqueueTime) > maxAge {
		pnq.logger.Warn("Notification expired in queue",
			"type", queuedNotif.Notification.Type,
			"priority", queuedNotif.Notification.Priority,
			"age", time.Since(queuedNotif.EnqueueTime))

		// Try to get next notification
		return pnq.Dequeue()
	}

	pnq.logger.Debug("Notification dequeued",
		"type", queuedNotif.Notification.Type,
		"priority", queuedNotif.Notification.Priority,
		"queue_time", time.Since(queuedNotif.EnqueueTime),
		"queue_size", pnq.currentSize)

	return queuedNotif.Notification
}

// Peek returns the highest priority notification without removing it
func (pnq *PriorityNotificationQueue) Peek() *Notification {
	pnq.mu.Lock()
	defer pnq.mu.Unlock()

	if pnq.heap.Len() == 0 {
		return nil
	}

	return (*pnq.heap)[0].Notification
}

// Size returns the current queue size
func (pnq *PriorityNotificationQueue) Size() int {
	pnq.mu.Lock()
	defer pnq.mu.Unlock()
	return pnq.currentSize
}

// IsEmpty returns true if the queue is empty
func (pnq *PriorityNotificationQueue) IsEmpty() bool {
	return pnq.Size() == 0
}

// Clear removes all notifications from the queue
func (pnq *PriorityNotificationQueue) Clear() {
	pnq.mu.Lock()
	defer pnq.mu.Unlock()

	clearedCount := pnq.heap.Len()
	pnq.heap = &NotificationHeap{}
	heap.Init(pnq.heap)
	pnq.currentSize = 0

	pnq.logger.Info("Priority queue cleared", "cleared_count", clearedCount)
}

// GetStats returns queue statistics
func (pnq *PriorityNotificationQueue) GetStats() map[string]interface{} {
	pnq.mu.Lock()
	defer pnq.mu.Unlock()

	// Calculate priority distribution
	priorityDist := make(map[int]int)
	for _, qn := range *pnq.heap {
		priorityDist[qn.Notification.Priority]++
	}

	// Calculate age statistics
	var totalAge time.Duration
	var maxAge time.Duration
	now := time.Now()

	for _, qn := range *pnq.heap {
		age := now.Sub(qn.EnqueueTime)
		totalAge += age
		if age > maxAge {
			maxAge = age
		}
	}

	avgAge := time.Duration(0)
	if pnq.currentSize > 0 {
		avgAge = totalAge / time.Duration(pnq.currentSize)
	}

	return map[string]interface{}{
		"current_size":          pnq.currentSize,
		"max_size":              pnq.maxSize,
		"total_enqueued":        pnq.totalEnqueued,
		"total_dequeued":        pnq.totalDequeued,
		"priority_distribution": priorityDist,
		"average_age":           avgAge.String(),
		"max_age":               maxAge.String(),
	}
}

// calculateQueuePriority converts notification priority to queue priority
// Higher queue priority = processed first
func (pnq *PriorityNotificationQueue) calculateQueuePriority(notificationPriority int) int {
	switch notificationPriority {
	case PriorityEmergency:
		return 1000
	case PriorityHigh:
		return 800
	case PriorityNormal:
		return 600
	case PriorityLow:
		return 400
	case PriorityLowest:
		return 200
	default:
		return 600 // Default to normal
	}
}

// getMaxAge returns maximum age a notification can stay in queue based on priority
func (pnq *PriorityNotificationQueue) getMaxAge(priority int) time.Duration {
	switch priority {
	case PriorityEmergency:
		return 5 * time.Minute
	case PriorityHigh:
		return 15 * time.Minute
	case PriorityNormal:
		return 30 * time.Minute
	case PriorityLow:
		return 1 * time.Hour
	case PriorityLowest:
		return 2 * time.Hour
	default:
		return 30 * time.Minute
	}
}

// Heap interface implementation for NotificationHeap

func (h NotificationHeap) Len() int {
	return len(h)
}

func (h NotificationHeap) Less(i, j int) bool {
	// Higher priority first
	if h[i].Priority != h[j].Priority {
		return h[i].Priority > h[j].Priority
	}

	// If same priority, older notifications first (FIFO within priority)
	return h[i].Sequence < h[j].Sequence
}

func (h NotificationHeap) Swap(i, j int) {
	h[i], h[j] = h[j], h[i]
}

func (h *NotificationHeap) Push(x interface{}) {
	*h = append(*h, x.(*QueuedNotification))
}

func (h *NotificationHeap) Pop() interface{} {
	old := *h
	n := len(old)
	item := old[n-1]
	old[n-1] = nil // avoid memory leak
	*h = old[0 : n-1]
	return item
}
