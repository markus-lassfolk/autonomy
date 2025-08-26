<template>
  <div>
    <tlt-form-item-input
      v-model="form.host"
      :label="$t('Starlink Host')"
      :help="$t('IP address or hostname of the Starlink dish')"
      type="text"
      required
    />
    
    <tlt-form-item-input
      v-model="form.port"
      :label="$t('Starlink Port')"
      :help="$t('Port number for Starlink API (default: 9200)')"
      type="number"
      min="1"
      max="65535"
      required
    />
    
    <tlt-form-item-input
      v-model="form.timeout_s"
      :label="$t('Timeout (seconds)')"
      :help="$t('Timeout for Starlink API requests')"
      type="number"
      min="1"
      max="60"
    />
    
    <tlt-form-item-switch
      v-model="form.grpc_first"
      :label="$t('gRPC First')"
      :help="$t('Try gRPC before HTTP for API calls')"
    />
    
    <tlt-form-item-switch
      v-model="form.http_first"
      :label="$t('HTTP First')"
      :help="$t('Try HTTP before gRPC for API calls')"
    />
    
    <tlt-form-item-input
      v-model="form.health_check_interval"
      :label="$t('Health Check Interval (s)')"
      :help="$t('Interval between Starlink health checks')"
      type="number"
      min="30"
      max="300"
    />
    
    <tlt-form-item-input
      v-model="form.obstruction_threshold"
      :label="$t('Obstruction Threshold (%)')"
      :help="$t('Threshold for obstruction warnings')"
      type="number"
      min="0"
      max="100"
      step="0.1"
    />
    
    <tlt-form-item-input
      v-model="form.latency_threshold"
      :label="$t('Latency Threshold (ms)')"
      :help="$t('Threshold for latency warnings')"
      type="number"
      min="10"
      max="1000"
    />
  </div>
</template>

<script>
export default {
  data() {
    return {
      form: {
        host: '192.168.100.1',
        port: '9200',
        timeout_s: '10',
        grpc_first: '1',
        http_first: '0',
        health_check_interval: '60',
        obstruction_threshold: '5.0',
        latency_threshold: '100'
      }
    };
  },
  
  mounted() {
    this.loadConfig();
  },
  
  methods: {
    async loadConfig() {
      try {
        const response = await this.$http.get('/api/autonomy/config/starlink');
        this.form = { ...this.form, ...response.data };
      } catch (error) {
        console.error('Failed to load Starlink config:', error);
      }
    },
    
    async saveConfig() {
      try {
        await this.$http.post('/api/autonomy/config/starlink', this.form);
        this.$message.success(this.$t('Starlink configuration saved'));
      } catch (error) {
        console.error('Failed to save Starlink config:', error);
        this.$message.error(this.$t('Failed to save Starlink configuration'));
      }
    }
  }
};
</script>
