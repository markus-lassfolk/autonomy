<template>
  <div>
    <tlt-card :title="$t('Autonomy Overview')">
      <div class="overview-grid">
        <div class="overview-card">
          <div class="card-header">
            <h3>{{ $t('Service Status') }}</h3>
          </div>
          <div class="card-content">
            <div class="status-indicator">
              <span :class="['status-dot', overview.running ? 'running' : 'stopped']"></span>
              <span class="status-text">{{ overview.running ? $t('Running') : $t('Stopped') }}</span>
            </div>
            <div class="detail-row">
              <span class="label">{{ $t('Uptime') }}:</span>
              <span class="value">{{ overview.uptime || $t('N/A') }}</span>
            </div>
            <div class="detail-row">
              <span class="label">{{ $t('Last Check') }}:</span>
              <span class="value">{{ overview.last_check || $t('N/A') }}</span>
            </div>
            <div class="detail-row">
              <span class="label">{{ $t('Issues Found') }}:</span>
              <span class="value" :class="{ 'has-issues': overview.issues > 0 }">{{ overview.issues }}</span>
            </div>
          </div>
        </div>

        <div class="overview-card">
          <div class="card-header">
            <h3>{{ $t('Services') }}</h3>
          </div>
          <div class="card-content">
            <div class="service-item">
              <span class="service-name">{{ $t('Starlink') }}</span>
              <span :class="['service-status', overview.services?.starlink ? 'enabled' : 'disabled']">
                {{ overview.services?.starlink ? $t('Enabled') : $t('Disabled') }}
              </span>
            </div>
            <div class="service-item">
              <span class="service-name">{{ $t('Cellular') }}</span>
              <span :class="['service-status', overview.services?.cellular ? 'enabled' : 'disabled']">
                {{ overview.services?.cellular ? $t('Enabled') : $t('Disabled') }}
              </span>
            </div>
            <div class="service-item">
              <span class="service-name">{{ $t('GPS') }}</span>
              <span :class="['service-status', overview.services?.gps ? 'enabled' : 'disabled']">
                {{ overview.services?.gps ? $t('Enabled') : $t('Disabled') }}
              </span>
            </div>
          </div>
        </div>
      </div>

      <div class="actions">
        <tlt-button @click="refreshOverview" :loading="refreshing">
          {{ $t('Refresh') }}
        </tlt-button>
        <tlt-button @click="restartService" :loading="restarting">
          {{ $t('Restart Service') }}
        </tlt-button>
      </div>
    </tlt-card>
  </div>
</template>

<script>
export default {
  data() {
    return {
      overview: {
        running: false,
        uptime: '',
        last_check: '',
        issues: 0,
        services: {
          starlink: false,
          cellular: false,
          gps: false
        }
      },
      refreshing: false,
      restarting: false
    }
  },
  mounted() {
    this.loadOverview()
    // Auto-refresh every 30 seconds
    this.interval = setInterval(() => {
      this.loadOverview()
    }, 30000)
  },
  beforeUnmount() {
    if (this.interval) {
      clearInterval(this.interval)
    }
  },
  methods: {
    async loadOverview() {
      try {
        const response = await this.$axios.get('/api/autonomy_f/overview')
        this.overview = response.data
      } catch (error) {
        console.error('Failed to load overview:', error)
        this.$message.error(this.$t('Failed to load overview'))
      }
    },
    async refreshOverview() {
      this.refreshing = true
      try {
        await this.loadOverview()
        this.$message.success(this.$t('Overview refreshed'))
      } catch (error) {
        console.error('Failed to refresh overview:', error)
        this.$message.error(this.$t('Failed to refresh overview'))
      } finally {
        this.refreshing = false
      }
    },
    async restartService() {
      this.restarting = true
      try {
        const response = await this.$axios.post('/api/autonomy_f/actions/restart')
        this.$message.success(this.$t('Service restarted successfully'))
        await this.loadOverview()
      } catch (error) {
        console.error('Failed to restart service:', error)
        this.$message.error(this.$t('Failed to restart service'))
      } finally {
        this.restarting = false
      }
    }
  }
}
</script>

<style scoped>
.overview-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
  gap: 16px;
  margin-bottom: 16px;
}

.overview-card {
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

.status-indicator {
  display: flex;
  align-items: center;
  margin-bottom: 16px;
}

.status-dot {
  width: 12px;
  height: 12px;
  border-radius: 50%;
  margin-right: 8px;
}

.status-dot.running {
  background-color: #52c41a;
}

.status-dot.stopped {
  background-color: #ff4d4f;
}

.status-text {
  font-weight: 600;
  font-size: 14px;
}

.detail-row {
  display: flex;
  justify-content: space-between;
  margin-bottom: 8px;
}

.detail-row .label {
  font-weight: 500;
  color: #666;
}

.detail-row .value {
  font-weight: 600;
  color: #333;
}

.detail-row .value.has-issues {
  color: #ff4d4f;
}

.service-item {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 12px;
  padding: 8px 0;
  border-bottom: 1px solid #f0f0f0;
}

.service-item:last-child {
  border-bottom: none;
  margin-bottom: 0;
}

.service-name {
  font-weight: 500;
  color: #333;
}

.service-status {
  padding: 4px 8px;
  border-radius: 4px;
  font-size: 12px;
  font-weight: 600;
}

.service-status.enabled {
  background-color: #f6ffed;
  color: #52c41a;
}

.service-status.disabled {
  background-color: #fff2e8;
  color: #fa8c16;
}

.actions {
  display: flex;
  gap: 8px;
  margin-top: 16px;
}
</style>





