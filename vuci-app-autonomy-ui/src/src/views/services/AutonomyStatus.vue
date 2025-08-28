<template>
  <div>
    <tlt-card :title="$t('Autonomy Status')">
      <div class="status-grid">
        <div class="status-card">
          <div class="card-header">
            <h3>{{ $t('System Status') }}</h3>
          </div>
          <div class="card-content">
            <div class="status-item">
              <span class="label">{{ $t('Daemon') }}:</span>
              <span :class="['value', status.daemon ? 'running' : 'stopped']">
                {{ status.daemon ? $t('Running') : $t('Stopped') }}
              </span>
            </div>
            <div class="status-item">
              <span class="label">{{ $t('Uptime') }}:</span>
              <span class="value">{{ status.uptime || $t('N/A') }}</span>
            </div>
            <div class="status-item">
              <span class="label">{{ $t('Last Check') }}:</span>
              <span class="value">{{ status.last_check || $t('N/A') }}</span>
            </div>
            <div class="status-item">
              <span class="label">{{ $t('Issues') }}:</span>
              <span class="value" :class="{ 'has-issues': status.issues > 0 }">{{ status.issues }}</span>
            </div>
          </div>
        </div>

        <div class="status-card">
          <div class="card-header">
            <h3>{{ $t('Starlink') }}</h3>
          </div>
          <div class="card-content">
            <div class="status-item" v-for="(value, key) in status.starlink" :key="key">
              <span class="label">{{ $t(key) }}:</span>
              <span class="value">{{ value }}</span>
            </div>
            <div v-if="Object.keys(status.starlink).length === 0" class="no-data">
              {{ $t('No Starlink data available') }}
            </div>
          </div>
        </div>

        <div class="status-card">
          <div class="card-header">
            <h3>{{ $t('Cellular') }}</h3>
          </div>
          <div class="card-content">
            <div class="status-item" v-for="(value, key) in status.cellular" :key="key">
              <span class="label">{{ $t(key) }}:</span>
              <span class="value">{{ value }}</span>
            </div>
            <div v-if="Object.keys(status.cellular).length === 0" class="no-data">
              {{ $t('No cellular data available') }}
            </div>
          </div>
        </div>

        <div class="status-card">
          <div class="card-header">
            <h3>{{ $t('GPS') }}</h3>
          </div>
          <div class="card-content">
            <div class="status-item" v-for="(value, key) in status.gps" :key="key">
              <span class="label">{{ $t(key) }}:</span>
              <span class="value">{{ value }}</span>
            </div>
            <div v-if="Object.keys(status.gps).length === 0" class="no-data">
              {{ $t('No GPS data available') }}
            </div>
          </div>
        </div>
      </div>

      <div class="actions">
        <tlt-button @click="refreshStatus" :loading="refreshing">
          {{ $t('Refresh') }}
        </tlt-button>
      </div>
    </tlt-card>
  </div>
</template>

<script>
export default {
  data() {
    return {
      status: {
        daemon: false,
        uptime: '',
        last_check: '',
        issues: 0,
        starlink: {},
        cellular: {},
        gps: {}
      },
      refreshing: false
    }
  },
  mounted() {
    this.loadStatus()
    // Auto-refresh every 10 seconds
    this.interval = setInterval(() => {
      this.loadStatus()
    }, 10000)
  },
  beforeUnmount() {
    if (this.interval) {
      clearInterval(this.interval)
    }
  },
  methods: {
    async loadStatus() {
      try {
        const response = await this.$axios.get('/api/autonomy_f/status')
        this.status = response.data
      } catch (error) {
        console.error('Failed to load status:', error)
        this.$message.error(this.$t('Failed to load status'))
      }
    },
    async refreshStatus() {
      this.refreshing = true
      try {
        await this.loadStatus()
        this.$message.success(this.$t('Status refreshed'))
      } catch (error) {
        console.error('Failed to refresh status:', error)
        this.$message.error(this.$t('Failed to refresh status'))
      } finally {
        this.refreshing = false
      }
    }
  }
}
</script>

<style scoped>
.status-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
  gap: 16px;
  margin-bottom: 16px;
}

.status-card {
  border: 1px solid #e8e8e8;
  border-radius: 8px;
  overflow: hidden;
}

.card-header {
  background-color: #f5f5f5;
  padding: 12px 16px;
  border-bottom: 1px solid #e8e8e8;
}

.card-header h3 {
  margin: 0;
  font-size: 16px;
  font-weight: 600;
  color: #333;
}

.card-content {
  padding: 16px;
}

.status-item {
  display: flex;
  justify-content: space-between;
  margin-bottom: 8px;
}

.status-item:last-child {
  margin-bottom: 0;
}

.status-item .label {
  font-weight: 500;
  color: #666;
}

.status-item .value {
  font-weight: 600;
  color: #333;
}

.status-item .value.running {
  color: #52c41a;
}

.status-item .value.stopped {
  color: #ff4d4f;
}

.status-item .value.has-issues {
  color: #ff4d4f;
}

.no-data {
  text-align: center;
  color: #999;
  font-style: italic;
  padding: 16px 0;
}

.actions {
  display: flex;
  gap: 8px;
  margin-top: 16px;
}
</style>





