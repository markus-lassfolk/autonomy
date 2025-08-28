<template>
  <div>
    <tlt-card :title="$t('Autonomy Logs')">
      <div class="log-controls">
        <tlt-button @click="refreshLogs" :loading="refreshing">
          {{ $t('Refresh') }}
        </tlt-button>
        <tlt-switch v-model="autoRefresh" :label="$t('Auto Refresh')" />
      </div>

      <div class="log-content">
        <div v-if="logs.length === 0" class="no-logs">
          {{ $t('No logs available') }}
        </div>
        <div v-else class="log-entries">
          <div 
            v-for="(log, index) in logs" 
            :key="index" 
            class="log-entry"
            :class="{ 'log-error': log.includes('ERROR'), 'log-warn': log.includes('WARN') }"
          >
            {{ log }}
          </div>
        </div>
      </div>
    </tlt-card>
  </div>
</template>

<script>
export default {
  data() {
    return {
      logs: [],
      refreshing: false,
      autoRefresh: true
    }
  },
  mounted() {
    this.loadLogs()
    // Auto-refresh every 5 seconds if enabled
    this.interval = setInterval(() => {
      if (this.autoRefresh) {
        this.loadLogs()
      }
    }, 5000)
  },
  beforeUnmount() {
    if (this.interval) {
      clearInterval(this.interval)
    }
  },
  methods: {
    async loadLogs() {
      try {
        const response = await this.$axios.get('/api/autonomy_f/logs')
        this.logs = response.data || []
      } catch (error) {
        console.error('Failed to load logs:', error)
        this.$message.error(this.$t('Failed to load logs'))
      }
    },
    async refreshLogs() {
      this.refreshing = true
      try {
        await this.loadLogs()
        this.$message.success(this.$t('Logs refreshed'))
      } catch (error) {
        console.error('Failed to refresh logs:', error)
        this.$message.error(this.$t('Failed to refresh logs'))
      } finally {
        this.refreshing = false
      }
    }
  }
}
</script>

<style scoped>
.log-controls {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 16px;
  padding: 16px;
  background-color: #f5f5f5;
  border-radius: 8px;
}

.log-content {
  border: 1px solid #e8e8e8;
  border-radius: 8px;
  overflow: hidden;
  max-height: 600px;
}

.no-logs {
  text-align: center;
  color: #999;
  font-style: italic;
  padding: 32px 16px;
}

.log-entries {
  max-height: 600px;
  overflow-y: auto;
}

.log-entry {
  padding: 8px 16px;
  border-bottom: 1px solid #f0f0f0;
  font-family: 'Courier New', monospace;
  font-size: 12px;
  line-height: 1.4;
  white-space: pre-wrap;
  word-break: break-all;
}

.log-entry:last-child {
  border-bottom: none;
}

.log-entry.log-error {
  background-color: #fff2f0;
  color: #cf1322;
}

.log-entry.log-warn {
  background-color: #fffbe6;
  color: #d48806;
}
</style>





