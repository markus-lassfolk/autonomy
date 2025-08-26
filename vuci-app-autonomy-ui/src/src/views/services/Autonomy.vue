<template>
  <div>
    <!-- System Status Dashboard -->
    <tlt-card :title="$t('System Status')" class="mb-4">
      <div class="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
        <div class="status-card" :class="systemStatus.status">
          <div class="status-icon">
            <i class="fas fa-server"></i>
          </div>
          <div class="status-content">
            <h3>{{ $t('System') }}</h3>
            <p>{{ systemStatus.status === 'healthy' ? $t('Healthy') : $t('Issues Detected') }}</p>
          </div>
        </div>
        
        <div class="status-card" :class="starlinkStatus.status">
          <div class="status-icon">
            <i class="fas fa-satellite"></i>
          </div>
          <div class="status-content">
            <h3>{{ $t('Starlink') }}</h3>
            <p>{{ starlinkStatus.latency }}ms</p>
          </div>
        </div>
        
        <div class="status-card" :class="gpsStatus.status">
          <div class="status-icon">
            <i class="fas fa-map-marker-alt"></i>
          </div>
          <div class="status-content">
            <h3>{{ $t('GPS') }}</h3>
            <p>{{ gpsStatus.fix ? $t('Fixed') : $t('No Fix') }}</p>
          </div>
        </div>
        
        <div class="status-card" :class="networkStatus.status">
          <div class="status-icon">
            <i class="fas fa-network-wired"></i>
          </div>
          <div class="status-content">
            <h3>{{ $t('Network') }}</h3>
            <p>{{ networkStatus.activeInterface }}</p>
          </div>
        </div>
      </div>
    </tlt-card>

    <!-- Real-time Monitoring -->
    <tlt-card :title="$t('Real-time Monitoring')" class="mb-4">
      <div class="grid grid-cols-1 lg:grid-cols-2 gap-4">
        <!-- Starlink Health -->
        <div class="monitoring-panel">
          <h4>{{ $t('Starlink Health') }}</h4>
          <div class="metrics-grid">
            <div class="metric">
              <label>{{ $t('Latency') }}</label>
              <span>{{ starlinkStatus.latency }}ms</span>
            </div>
            <div class="metric">
              <label>{{ $t('Obstruction') }}</label>
              <span>{{ starlinkStatus.obstruction }}%</span>
            </div>
            <div class="metric">
              <label>{{ $t('SNR') }}</label>
              <span>{{ starlinkStatus.snr }}dB</span>
            </div>
            <div class="metric">
              <label>{{ $t('Uptime') }}</label>
              <span>{{ starlinkStatus.uptime }}h</span>
            </div>
          </div>
        </div>

        <!-- GPS Information -->
        <div class="monitoring-panel">
          <h4>{{ $t('GPS Information') }}</h4>
          <div class="metrics-grid">
            <div class="metric">
              <label>{{ $t('Latitude') }}</label>
              <span>{{ gpsStatus.latitude }}</span>
            </div>
            <div class="metric">
              <label>{{ $t('Longitude') }}</label>
              <span>{{ gpsStatus.longitude }}</span>
            </div>
            <div class="metric">
              <label>{{ $t('Altitude') }}</label>
              <span>{{ gpsStatus.altitude }}m</span>
            </div>
            <div class="metric">
              <label>{{ $t('Satellites') }}</label>
              <span>{{ gpsStatus.satellites }}</span>
            </div>
          </div>
        </div>
      </div>
    </tlt-card>

    <!-- Control Actions -->
    <tlt-card :title="$t('Control Actions')" class="mb-4">
      <div class="grid grid-cols-1 md:grid-cols-3 gap-4">
        <tlt-button @click="refreshStatus" :loading="loading">
          <i class="fas fa-sync-alt mr-2"></i>
          {{ $t('Refresh Status') }}
        </tlt-button>
        
        <tlt-button @click="runHealthCheck" :loading="healthCheckLoading">
          <i class="fas fa-heartbeat mr-2"></i>
          {{ $t('Run Health Check') }}
        </tlt-button>
        
        <tlt-button @click="submitOpenCellID" :loading="opencellidLoading">
          <i class="fas fa-upload mr-2"></i>
          {{ $t('Submit to OpenCELLID') }}
        </tlt-button>
      </div>
    </tlt-card>

    <!-- Configuration -->
    <vuci-form
      v-slot="{ uciData }"
      v-model="cAutonomyForm"
      config="autonomy"
      async-load
    >
      <vuci-typed-section
        type="main"
        data-key="main"
        :title="$t('Core Configuration')"
        :columns="mainConfigColumns"
        :edit-form="mainEditModal"
        :endpoints="[{ endpoint: 'autonomy_c/config' }]"
        :uci-data="uciData"
      >
        <template #enable="{ s }">
          <vuci-form-item-switch :uci-section="s" name="enable" />
        </template>
        <template #log_level="{ s }">
          <vuci-form-item-select :uci-section="s" name="log_level" :options="logLevelOptions" />
        </template>
        <template #poll_interval_ms="{ s }">
          <vuci-form-item-input :uci-section="s" name="poll_interval_ms" type="number" />
        </template>
      </vuci-typed-section>

      <!-- Starlink Configuration -->
      <vuci-typed-section
        type="starlink"
        data-key="starlink"
        :title="$t('Starlink Configuration')"
        :columns="starlinkConfigColumns"
        :edit-form="starlinkEditModal"
        :endpoints="[{ endpoint: 'autonomy_c/config' }]"
        :uci-data="uciData"
      >
        <template #host="{ s }">
          <vuci-form-item-input :uci-section="s" name="host" />
        </template>
        <template #port="{ s }">
          <vuci-form-item-input :uci-section="s" name="port" type="number" />
        </template>
        <template #timeout_s="{ s }">
          <vuci-form-item-input :uci-section="s" name="timeout_s" type="number" />
        </template>
      </vuci-typed-section>

      <!-- GPS Configuration -->
      <vuci-typed-section
        type="gps"
        data-key="gps"
        :title="$t('GPS & Location Configuration')"
        :columns="gpsConfigColumns"
        :edit-form="gpsEditModal"
        :endpoints="[{ endpoint: 'autonomy_c/config' }]"
        :uci-data="uciData"
      >
        <template #enabled="{ s }">
          <vuci-form-item-switch :uci-section="s" name="enabled" />
        </template>
        <template #google_api_key="{ s }">
          <vuci-form-item-input :uci-section="s" name="google_api_key" type="password" />
        </template>
        <template #opencellid_api_key="{ s }">
          <vuci-form-item-input :uci-section="s" name="opencellid_api_key" type="password" />
        </template>
        <template #submit_to_opencellid="{ s }">
          <vuci-form-item-switch :uci-section="s" name="submit_to_opencellid" />
        </template>
      </vuci-typed-section>
    </vuci-form>

    <!-- Logs Viewer -->
    <tlt-card :title="$t('System Logs')" class="mt-4">
      <div class="log-controls mb-4">
        <tlt-button @click="refreshLogs" :loading="logsLoading">
          <i class="fas fa-sync-alt mr-2"></i>
          {{ $t('Refresh Logs') }}
        </tlt-button>
        <tlt-button @click="clearLogs" class="ml-2">
          <i class="fas fa-trash mr-2"></i>
          {{ $t('Clear Logs') }}
        </tlt-button>
      </div>
      
      <div class="log-tabs">
        <button 
          v-for="logType in logTypes" 
          :key="logType.key"
          @click="activeLog = logType.key"
          :class="['log-tab', { active: activeLog === logType.key }]"
        >
          {{ logType.label }}
        </button>
      </div>
      
      <div class="log-content">
        <pre v-if="logs[activeLog]" class="log-text">{{ logs[activeLog] }}</pre>
        <div v-else class="log-empty">{{ $t('No logs available') }}</div>
      </div>
    </tlt-card>
  </div>
</template>

<script>
import { markRaw } from "vue";
import MainEditForm from "./AutonomyMainEdit.vue";
import StarlinkEditForm from "./AutonomyStarlinkEdit.vue";
import GPSEditForm from "./AutonomyGPSEdit.vue";

export default {
  data() {
    return {
      loading: false,
      healthCheckLoading: false,
      opencellidLoading: false,
      logsLoading: false,
      
      systemStatus: {
        status: 'unknown',
        lastCheck: null
      },
      
      starlinkStatus: {
        status: 'unknown',
        latency: 0,
        obstruction: 0,
        snr: 0,
        uptime: 0
      },
      
      gpsStatus: {
        status: 'unknown',
        fix: false,
        latitude: 0,
        longitude: 0,
        altitude: 0,
        satellites: 0
      },
      
      networkStatus: {
        status: 'unknown',
        activeInterface: 'unknown'
      },
      
      cAutonomyForm: {},
      
      mainConfigColumns: [
        { key: 'enable', label: this.$t('Enabled') },
        { key: 'log_level', label: this.$t('Log Level') },
        { key: 'poll_interval_ms', label: this.$t('Poll Interval (ms)') }
      ],
      
      starlinkConfigColumns: [
        { key: 'host', label: this.$t('Host') },
        { key: 'port', label: this.$t('Port') },
        { key: 'timeout_s', label: this.$t('Timeout (s)') }
      ],
      
      gpsConfigColumns: [
        { key: 'enabled', label: this.$t('Enabled') },
        { key: 'submit_to_opencellid', label: this.$t('Submit to OpenCELLID') },
        { key: 'cache_ttl_hours', label: this.$t('Cache TTL (h)') }
      ],
      
      logLevelOptions: [
        { value: 'debug', label: this.$t('Debug') },
        { value: 'info', label: this.$t('Info') },
        { value: 'warn', label: this.$t('Warning') },
        { value: 'error', label: this.$t('Error') }
      ],
      
      mainEditModal: markRaw(MainEditForm),
      starlinkEditModal: markRaw(StarlinkEditForm),
      gpsEditModal: markRaw(GPSEditForm),
      
      activeLog: 'autonomy',
      logTypes: [
        { key: 'autonomy', label: this.$t('Autonomy') },
        { key: 'gps', label: this.$t('GPS') },
        { key: 'opencellid', label: this.$t('OpenCELLID') },
        { key: 'health', label: this.$t('Health') }
      ],
      
      logs: {
        autonomy: '',
        gps: '',
        opencellid: '',
        health: ''
      }
    };
  },
  
  mounted() {
    this.refreshStatus();
    this.refreshLogs();
    
    // Auto-refresh every 30 seconds
    this.interval = setInterval(() => {
      this.refreshStatus();
    }, 30000);
  },
  
  beforeUnmount() {
    if (this.interval) {
      clearInterval(this.interval);
    }
  },
  
  methods: {
    async refreshStatus() {
      this.loading = true;
      try {
        // Get system status
        const statusResponse = await this.$http.get('/api/autonomy/status');
        this.systemStatus = statusResponse.data;
        
        // Get Starlink status
        const starlinkResponse = await this.$http.get('/api/autonomy/starlink');
        this.starlinkStatus = starlinkResponse.data;
        
        // Get GPS status
        const gpsResponse = await this.$http.get('/api/autonomy/gps');
        this.gpsStatus = gpsResponse.data;
        
        // Get network status
        const networkResponse = await this.$http.get('/api/autonomy/network');
        this.networkStatus = networkResponse.data;
        
      } catch (error) {
        console.error('Failed to refresh status:', error);
        this.$message.error(this.$t('Failed to refresh status'));
      } finally {
        this.loading = false;
      }
    },
    
    async runHealthCheck() {
      this.healthCheckLoading = true;
      try {
        const response = await this.$http.post('/api/autonomy/health-check');
        this.$message.success(this.$t('Health check completed'));
        await this.refreshStatus();
      } catch (error) {
        console.error('Health check failed:', error);
        this.$message.error(this.$t('Health check failed'));
      } finally {
        this.healthCheckLoading = false;
      }
    },
    
    async submitOpenCellID() {
      this.opencellidLoading = true;
      try {
        const response = await this.$http.post('/api/autonomy/opencellid/submit');
        this.$message.success(this.$t('Data submitted to OpenCELLID'));
        await this.refreshLogs();
      } catch (error) {
        console.error('OpenCELLID submission failed:', error);
        this.$message.error(this.$t('OpenCELLID submission failed'));
      } finally {
        this.opencellidLoading = false;
      }
    },
    
    async refreshLogs() {
      this.logsLoading = true;
      try {
        const logPromises = this.logTypes.map(async (logType) => {
          try {
            const response = await this.$http.get(`/api/autonomy/logs/${logType.key}`);
            this.logs[logType.key] = response.data.content || '';
          } catch (error) {
            this.logs[logType.key] = this.$t('Log file not available');
          }
        });
        
        await Promise.all(logPromises);
      } catch (error) {
        console.error('Failed to refresh logs:', error);
        this.$message.error(this.$t('Failed to refresh logs'));
      } finally {
        this.logsLoading = false;
      }
    },
    
    async clearLogs() {
      try {
        await this.$http.post('/api/autonomy/logs/clear');
        this.$message.success(this.$t('Logs cleared'));
        await this.refreshLogs();
      } catch (error) {
        console.error('Failed to clear logs:', error);
        this.$message.error(this.$t('Failed to clear logs'));
      }
    }
  }
};
</script>

<style scoped>
.status-card {
  @apply p-4 rounded-lg border flex items-center;
}

.status-card.healthy {
  @apply bg-green-50 border-green-200 text-green-800;
}

.status-card.warning {
  @apply bg-yellow-50 border-yellow-200 text-yellow-800;
}

.status-card.error {
  @apply bg-red-50 border-red-200 text-red-800;
}

.status-card.unknown {
  @apply bg-gray-50 border-gray-200 text-gray-800;
}

.status-icon {
  @apply text-2xl mr-3;
}

.status-content h3 {
  @apply font-semibold text-sm;
}

.status-content p {
  @apply text-xs opacity-75;
}

.monitoring-panel {
  @apply p-4 border rounded-lg;
}

.monitoring-panel h4 {
  @apply font-semibold mb-3 text-gray-700;
}

.metrics-grid {
  @apply grid grid-cols-2 gap-3;
}

.metric {
  @apply flex flex-col;
}

.metric label {
  @apply text-xs text-gray-600 mb-1;
}

.metric span {
  @apply font-mono text-sm font-semibold;
}

.log-controls {
  @apply flex items-center;
}

.log-tabs {
  @apply flex border-b mb-4;
}

.log-tab {
  @apply px-4 py-2 text-sm border-b-2 border-transparent cursor-pointer;
}

.log-tab.active {
  @apply border-blue-500 text-blue-600;
}

.log-content {
  @apply bg-gray-50 rounded p-4 max-h-96 overflow-auto;
}

.log-text {
  @apply text-xs font-mono whitespace-pre-wrap;
}

.log-empty {
  @apply text-gray-500 text-center py-8;
}
</style>
