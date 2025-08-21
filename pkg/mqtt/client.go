package mqtt

import (
	"encoding/json"
	"fmt"
	"sync"
	"time"

	MQTT "github.com/eclipse/paho.mqtt.golang"
	"github.com/markus-lassfolk/autonomy/pkg/logx"
)

// Client provides MQTT publishing for autonomyd telemetry with network optimization
type Client struct {
	client      MQTT.Client
	logger      *logx.Logger
	config      *Config
	connected   bool
	lastPublish time.Time

	// Network optimization: Connection pooling and reuse
	connectionPool map[string]*ConnectionInfo

	// Network optimization: Message batching
	messageQueue   []*QueuedMessage
	queueMutex     sync.Mutex
	queueSize      int
	maxQueueSize   int
	batchInterval  time.Duration
	lastBatchFlush time.Time

	// Network optimization: Rate limiting
	publishRateLimiter *RateLimiter
}

// Config holds MQTT configuration
type Config struct {
	Broker      string `json:"broker"`
	Port        int    `json:"port"`
	ClientID    string `json:"client_id"`
	Username    string `json:"username"`
	Password    string `json:"password"`
	TopicPrefix string `json:"topic_prefix"`
	QoS         int    `json:"qos"`
	Retain      bool   `json:"retain"`
	Enabled     bool   `json:"enabled"`
}

// DefaultConfig returns default MQTT configuration
func DefaultConfig() *Config {
	return &Config{
		Broker:      "localhost",
		Port:        1883,
		ClientID:    "autonomyd",
		TopicPrefix: "autonomy",
		QoS:         1,
		Retain:      false,
		Enabled:     false,
	}
}

// NewClient creates a new MQTT client with network optimization
func NewClient(config *Config, logger *logx.Logger) *Client {
	return &Client{
		logger: logger,
		config: config,

		// Network optimization: Initialize connection pool
		connectionPool: make(map[string]*ConnectionInfo),

		// Network optimization: Initialize message batching
		messageQueue:  make([]*QueuedMessage, 0, 100),
		maxQueueSize:  100,
		batchInterval: 5 * time.Second, // Batch messages for 5 seconds

		// Network optimization: Initialize rate limiting
		publishRateLimiter: &RateLimiter{
			maxMessages: 10,              // Max 10 messages per window
			windowSize:  1 * time.Second, // 1 second window
		},
	}
}

// Connect establishes connection to MQTT broker
func (c *Client) Connect() error {
	if !c.config.Enabled {
		c.logger.Debug("MQTT client disabled")
		return nil
	}

	opts := MQTT.NewClientOptions()
	opts.AddBroker(fmt.Sprintf("tcp://%s:%d", c.config.Broker, c.config.Port))
	opts.SetClientID(c.config.ClientID)

	if c.config.Username != "" {
		opts.SetUsername(c.config.Username)
		opts.SetPassword(c.config.Password)
	}

	opts.SetAutoReconnect(true)
	opts.SetConnectRetry(true)
	opts.SetConnectRetryInterval(5 * time.Second)
	opts.SetMaxReconnectInterval(1 * time.Minute)

	opts.SetOnConnectHandler(c.onConnect)
	opts.SetConnectionLostHandler(c.onConnectionLost)
	opts.SetDefaultPublishHandler(c.onMessageReceived)

	c.client = MQTT.NewClient(opts)

	if token := c.client.Connect(); token.Wait() && token.Error() != nil {
		return fmt.Errorf("failed to connect to MQTT broker: %w", token.Error())
	}

	c.logger.Info("MQTT client connected", map[string]interface{}{
		"broker": c.config.Broker,
		"port":   c.config.Port,
	})

	return nil
}

// Disconnect disconnects from MQTT broker
func (c *Client) Disconnect() error {
	if c.client != nil && c.connected {
		c.client.Disconnect(250)
		c.connected = false
		c.logger.Info("MQTT client disconnected")
	}
	return nil
}

// onConnect handles MQTT connection events
func (c *Client) onConnect(client MQTT.Client) {
	c.connected = true
	c.logger.Info("MQTT connection established")
}

// onConnectionLost handles MQTT disconnection events
func (c *Client) onConnectionLost(client MQTT.Client, err error) {
	c.connected = false
	c.logger.Error("MQTT connection lost", map[string]interface{}{
		"error": err.Error(),
	})
}

// onMessageReceived handles incoming MQTT messages
func (c *Client) onMessageReceived(client MQTT.Client, msg MQTT.Message) {
	c.logger.Debug("MQTT message received", map[string]interface{}{
		"topic":   msg.Topic(),
		"payload": string(msg.Payload()),
	})
}

// PublishSample publishes a member sample to MQTT
func (c *Client) PublishSample(sample interface{}) error {
	if !c.config.Enabled || !c.connected {
		return nil
	}

	// Use reflection or type assertion to access sample fields
	// For now, we'll use a generic approach
	topic := fmt.Sprintf("%s/members/sample", c.config.TopicPrefix)

	payload := map[string]interface{}{
		"timestamp": time.Now(),
		"sample":    sample,
	}

	return c.publishJSON(topic, payload)
}

// PublishEvent publishes an event to MQTT
func (c *Client) PublishEvent(event interface{}) error {
	if !c.config.Enabled || !c.connected {
		return nil
	}

	topic := fmt.Sprintf("%s/events", c.config.TopicPrefix)

	payload := map[string]interface{}{
		"timestamp": time.Now(),
		"event":     event,
	}

	return c.publishJSON(topic, payload)
}

// PublishStatus publishes system status to MQTT
func (c *Client) PublishStatus(status map[string]interface{}) error {
	if !c.config.Enabled || !c.connected {
		return nil
	}

	topic := fmt.Sprintf("%s/status", c.config.TopicPrefix)

	payload := map[string]interface{}{
		"timestamp": time.Now(),
		"status":    status,
	}

	return c.publishJSON(topic, payload)
}

// PublishMemberList publishes the current member list to MQTT
func (c *Client) PublishMemberList(members interface{}) error {
	if !c.config.Enabled || !c.connected {
		return nil
	}

	topic := fmt.Sprintf("%s/members", c.config.TopicPrefix)

	payload := map[string]interface{}{
		"timestamp": time.Now(),
		"members":   members,
	}

	return c.publishJSON(topic, payload)
}

// PublishHealth publishes health information to MQTT
func (c *Client) PublishHealth(health map[string]interface{}) error {
	if !c.config.Enabled || !c.connected {
		return nil
	}

	topic := fmt.Sprintf("%s/health", c.config.TopicPrefix)

	payload := map[string]interface{}{
		"timestamp": time.Now(),
		"health":    health,
	}

	return c.publishJSON(topic, payload)
}

// publishJSON publishes JSON payload to MQTT topic
func (c *Client) publishJSON(topic string, payload interface{}) error {
	data, err := json.Marshal(payload)
	if err != nil {
		return fmt.Errorf("failed to marshal JSON: %w", err)
	}

	token := c.client.Publish(topic, byte(c.config.QoS), c.config.Retain, data)
	if token.Wait() && token.Error() != nil {
		return fmt.Errorf("failed to publish to topic %s: %w", topic, token.Error())
	}

	c.lastPublish = time.Now()
	c.logger.Debug("MQTT message published", map[string]interface{}{
		"topic": topic,
		"size":  len(data),
	})

	return nil
}

// IsConnected returns whether the MQTT client is connected
func (c *Client) IsConnected() bool {
	return c.connected && c.client != nil && c.client.IsConnected()
}

// GetLastPublish returns the timestamp of the last publish
func (c *Client) GetLastPublish() time.Time {
	return c.lastPublish
}

// Subscribe subscribes to an MQTT topic
func (c *Client) Subscribe(topic string, handler MQTT.MessageHandler) error {
	if !c.config.Enabled || !c.connected {
		return nil
	}

	token := c.client.Subscribe(topic, byte(c.config.QoS), handler)
	if token.Wait() && token.Error() != nil {
		return fmt.Errorf("failed to subscribe to topic %s: %w", topic, token.Error())
	}

	c.logger.Info("MQTT subscription created", map[string]interface{}{
		"topic": topic,
	})

	return nil
}

// Unsubscribe unsubscribes from an MQTT topic
func (c *Client) Unsubscribe(topic string) error {
	if !c.config.Enabled || !c.connected {
		return nil
	}

	token := c.client.Unsubscribe(topic)
	if token.Wait() && token.Error() != nil {
		return fmt.Errorf("failed to unsubscribe from topic %s: %w", topic, token.Error())
	}

	c.logger.Info("MQTT subscription removed", map[string]interface{}{
		"topic": topic,
	})

	return nil
}

// PublishWithRetry publishes with retry logic
func (c *Client) PublishWithRetry(topic string, payload interface{}, maxRetries int) error {
	var lastErr error

	for i := 0; i < maxRetries; i++ {
		if err := c.publishJSON(topic, payload); err != nil {
			lastErr = err
			c.logger.Warn("MQTT publish failed, retrying", map[string]interface{}{
				"topic":       topic,
				"attempt":     i + 1,
				"max_retries": maxRetries,
				"error":       err.Error(),
			})

			// Wait before retry
			time.Sleep(time.Duration(i+1) * time.Second)
			continue
		}

		// Success
		return nil
	}

	return fmt.Errorf("failed to publish after %d retries: %w", maxRetries, lastErr)
}

// Publish publishes a message with network optimization
func (c *Client) Publish(topic string, payload interface{}) error {
	if !c.config.Enabled {
		return nil
	}

	// Network optimization: Check rate limiting
	if !c.publishRateLimiter.Allow() {
		c.logger.Debug("Rate limit exceeded, queuing message", "topic", topic)
		return c.queueMessage(topic, payload)
	}

	// Convert payload to JSON
	data, err := json.Marshal(payload)
	if err != nil {
		return fmt.Errorf("failed to marshal payload: %w", err)
	}

	// Network optimization: Use batched publishing
	return c.publishBatched(topic, data)
}

// publishBatched publishes messages with batching for network efficiency
func (c *Client) publishBatched(topic string, payload []byte) error {
	c.queueMutex.Lock()
	defer c.queueMutex.Unlock()

	// Add to queue
	queuedMsg := &QueuedMessage{
		Topic:   topic,
		Payload: payload,
		QoS:     c.config.QoS,
		Retain:  c.config.Retain,
		Time:    time.Now(),
	}

	c.messageQueue = append(c.messageQueue, queuedMsg)
	c.queueSize++

	// Flush if queue is full or batch interval has passed
	if c.queueSize >= c.maxQueueSize || time.Since(c.lastBatchFlush) >= c.batchInterval {
		return c.flushMessageQueue()
	}

	return nil
}

// queueMessage adds a message to the queue when rate limited
func (c *Client) queueMessage(topic string, payload interface{}) error {
	data, err := json.Marshal(payload)
	if err != nil {
		return fmt.Errorf("failed to marshal payload: %w", err)
	}

	c.queueMutex.Lock()
	defer c.queueMutex.Unlock()

	if c.queueSize < c.maxQueueSize {
		queuedMsg := &QueuedMessage{
			Topic:   topic,
			Payload: data,
			QoS:     c.config.QoS,
			Retain:  c.config.Retain,
			Time:    time.Now(),
		}
		c.messageQueue = append(c.messageQueue, queuedMsg)
		c.queueSize++
	} else {
		c.logger.Warn("Message queue full, dropping message", "topic", topic)
	}

	return nil
}

// flushMessageQueue publishes all queued messages in a batch
func (c *Client) flushMessageQueue() error {
	if len(c.messageQueue) == 0 {
		return nil
	}

	// Network optimization: Publish all messages in batch
	for _, msg := range c.messageQueue {
		if err := c.publishDirect(msg.Topic, msg.Payload); err != nil {
			c.logger.Error("Failed to publish queued message", "topic", msg.Topic, "error", err)
			// Continue with other messages
		}
	}

	// Clear queue
	c.messageQueue = c.messageQueue[:0]
	c.queueSize = 0
	c.lastBatchFlush = time.Now()

	return nil
}

// publishDirect publishes a single message directly
func (c *Client) publishDirect(topic string, payload []byte) error {
	if !c.connected {
		return fmt.Errorf("not connected to MQTT broker")
	}

	token := c.client.Publish(topic, byte(c.config.QoS), c.config.Retain, payload)
	if token.Wait() && token.Error() != nil {
		return fmt.Errorf("failed to publish message: %w", token.Error())
	}

	return nil
}

// Allow checks if a rate limit allows publishing
func (rl *RateLimiter) Allow() bool {
	rl.mu.Lock()
	defer rl.mu.Unlock()

	now := time.Now()

	// Reset counter if window has passed
	if now.Sub(rl.lastCheck) >= rl.windowSize {
		rl.messageCount = 0
		rl.lastCheck = now
	}

	// Check if we can allow another message
	if rl.messageCount < rl.maxMessages {
		rl.messageCount++
		return true
	}

	return false
}

// ConnectionInfo represents a pooled connection
type ConnectionInfo struct {
	Client     MQTT.Client
	LastUsed   time.Time
	Healthy    bool
	ErrorCount int
}

// QueuedMessage represents a message waiting to be published
type QueuedMessage struct {
	Topic   string
	Payload []byte
	QoS     int
	Retain  bool
	Time    time.Time
}

// RateLimiter implements rate limiting for network operations
type RateLimiter struct {
	mu           sync.Mutex
	lastCheck    time.Time
	messageCount int
	maxMessages  int
	windowSize   time.Duration
}
