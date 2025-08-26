<template>
  <div>
    <tlt-form-item-switch
      v-model="form.enable"
      :label="$t('Enable Autonomy')"
      :help="$t('Enable or disable the autonomy system')"
    />
    
    <tlt-form-item-select
      v-model="form.log_level"
      :label="$t('Log Level')"
      :help="$t('Set the logging level for the system')"
      :options="logLevelOptions"
    />
    
    <tlt-form-item-input
      v-model="form.log_file"
      :label="$t('Log File Path')"
      :help="$t('Path to the main log file (leave empty for default)')"
      type="text"
    />
    
    <tlt-form-item-input
      v-model="form.poll_interval_ms"
      :label="$t('Poll Interval (ms)')"
      :help="$t('Interval between health checks in milliseconds')"
      type="number"
      min="1000"
      max="60000"
    />
    
    <tlt-form-item-input
      v-model="form.history_window_s"
      :label="$t('History Window (s)')"
      :help="$t('Time window for historical data in seconds')"
      type="number"
      min="60"
      max="86400"
    />
    
    <tlt-form-item-input
      v-model="form.min_uptime_s"
      :label="$t('Minimum Uptime (s)')"
      :help="$t('Minimum uptime before considering interface stable')"
      type="number"
      min="10"
      max="300"
    />
    
    <tlt-form-item-input
      v-model="form.cooldown_s"
      :label="$t('Cooldown Period (s)')"
      :help="$t('Cooldown period between failover attempts')"
      type="number"
      min="10"
      max="600"
    />
    
    <tlt-form-item-switch
      v-model="form.use_mwan3"
      :label="$t('Use mwan3')"
      :help="$t('Use mwan3 for multi-WAN management')"
    />
    
    <tlt-form-item-switch
      v-model="form.predictive"
      :label="$t('Predictive Failover')"
      :help="$t('Enable predictive failover based on trends')"
    />
    
    <tlt-form-item-input
      v-model="form.switch_margin"
      :label="$t('Switch Margin (%)')"
      :help="$t('Performance margin for switching interfaces')"
      type="number"
      min="5"
      max="50"
    />
    
    <tlt-form-item-select
      v-model="form.data_cap_mode"
      :label="$t('Data Cap Mode')"
      :help="$t('How to handle data caps and limits')"
      :options="dataCapOptions"
    />
  </div>
</template>

<script>
export default {
  data() {
    return {
      form: {
        enable: '1',
        log_level: 'info',
        log_file: '',
        poll_interval_ms: '2000',
        history_window_s: '600',
        min_uptime_s: '30',
        cooldown_s: '30',
        use_mwan3: '1',
        predictive: '1',
        switch_margin: '15',
        data_cap_mode: 'balanced'
      },
      
      logLevelOptions: [
        { value: 'debug', label: this.$t('Debug') },
        { value: 'info', label: this.$t('Info') },
        { value: 'warn', label: this.$t('Warning') },
        { value: 'error', label: this.$t('Error') }
      ],
      
      dataCapOptions: [
        { value: 'balanced', label: this.$t('Balanced') },
        { value: 'conservative', label: this.$t('Conservative') },
        { value: 'aggressive', label: this.$t('Aggressive') }
      ]
    };
  },
  
  mounted() {
    // Load current configuration
    this.loadConfig();
  },
  
  methods: {
    async loadConfig() {
      try {
        const response = await this.$http.get('/api/autonomy/config/main');
        this.form = { ...this.form, ...response.data };
      } catch (error) {
        console.error('Failed to load config:', error);
      }
    },
    
    async saveConfig() {
      try {
        await this.$http.post('/api/autonomy/config/main', this.form);
        this.$message.success(this.$t('Configuration saved'));
      } catch (error) {
        console.error('Failed to save config:', error);
        this.$message.error(this.$t('Failed to save configuration'));
      }
    }
  }
};
</script>
