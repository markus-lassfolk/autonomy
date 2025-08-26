<template>
  <div>
    <tlt-form-item-switch
      v-model="form.enabled"
      :label="$t('Enable GPS')"
      :help="$t('Enable GPS and location services')"
    />
    
    <tlt-form-item-input
      v-model="form.google_api_key"
      :label="$t('Google Location API Key')"
      :help="$t('API key for Google Location Services')"
      type="password"
      placeholder="Enter your Google API key"
    />
    
    <tlt-form-item-input
      v-model="form.opencellid_api_key"
      :label="$t('OpenCELLID API Key')"
      :help="$t('API key for OpenCELLID data submission')"
      type="password"
      placeholder="Enter your OpenCELLID API key"
    />
    
    <tlt-form-item-switch
      v-model="form.submit_to_opencellid"
      :label="$t('Submit to OpenCELLID')"
      :help="$t('Automatically submit location data to OpenCELLID')"
    />
    
    <tlt-form-item-input
      v-model="form.cache_ttl_hours"
      :label="$t('Cache TTL (hours)')"
      :help="$t('Time to live for location cache in hours')"
      type="number"
      min="1"
      max="168"
    />
    
    <tlt-form-item-input
      v-model="form.gps_device"
      :label="$t('GPS Device')"
      :help="$t('Path to GPS device (e.g., /dev/ttyUSB0)')"
      type="text"
    />
    
    <tlt-form-item-input
      v-model="form.gps_baudrate"
      :label="$t('GPS Baud Rate')"
      :help="$t('Baud rate for GPS device communication')"
      type="number"
      min="9600"
      max="115200"
    />
    
    <tlt-form-item-select
      v-model="form.location_services"
      :label="$t('Location Services')"
      :help="$t('Comma-separated list of location services to use')"
      :options="locationServiceOptions"
      multiple
    />
    
    <tlt-form-item-input
      v-model="form.submit_interval_min"
      :label="$t('Submit Interval (minutes)')"
      :help="$t('Interval for OpenCELLID data submission')"
      type="number"
      min="5"
      max="1440"
    />
    
    <tlt-form-item-switch
      v-model="form.include_gps"
      :label="$t('Include GPS Data')"
      :help="$t('Include GPS coordinates in OpenCELLID submissions')"
    />
    
    <tlt-form-item-switch
      v-model="form.include_cell"
      :label="$t('Include Cell Data')"
      :help="$t('Include cellular tower data in OpenCELLID submissions')"
    />
    
    <tlt-form-item-switch
      v-model="form.include_wifi"
      :label="$t('Include WiFi Data')"
      :help="$t('Include WiFi access point data in OpenCELLID submissions')"
    />
    
    <tlt-form-item-switch
      v-model="form.fallback_enabled"
      :label="$t('Enable Fallback')"
      :help="$t('Enable fallback to other location services when GPS fails')"
    />
    
    <tlt-form-item-switch
      v-model="form.cache_enabled"
      :label="$t('Enable Caching')"
      :help="$t('Cache location data to reduce API calls')"
    />
  </div>
</template>

<script>
export default {
  data() {
    return {
      form: {
        enabled: '1',
        google_api_key: '',
        opencellid_api_key: '',
        submit_to_opencellid: '1',
        cache_ttl_hours: '24',
        gps_device: '/dev/ttyUSB0',
        gps_baudrate: '9600',
        location_services: 'gps,google,opencellid',
        submit_interval_min: '30',
        include_gps: '1',
        include_cell: '1',
        include_wifi: '1',
        fallback_enabled: '1',
        cache_enabled: '1'
      },
      
      locationServiceOptions: [
        { value: 'gps', label: this.$t('GPS') },
        { value: 'google', label: this.$t('Google Location') },
        { value: 'opencellid', label: this.$t('OpenCELLID') },
        { value: 'starlink', label: this.$t('Starlink Location') }
      ]
    };
  },
  
  mounted() {
    this.loadConfig();
  },
  
  methods: {
    async loadConfig() {
      try {
        const response = await this.$http.get('/api/autonomy/config/gps');
        this.form = { ...this.form, ...response.data };
      } catch (error) {
        console.error('Failed to load GPS config:', error);
      }
    },
    
    async saveConfig() {
      try {
        await this.$http.post('/api/autonomy/config/gps', this.form);
        this.$message.success(this.$t('GPS configuration saved'));
      } catch (error) {
        console.error('Failed to save GPS config:', error);
        this.$message.error(this.$t('Failed to save GPS configuration'));
      }
    },
    
    async testGPS() {
      try {
        const response = await this.$http.post('/api/autonomy/gps/test');
        this.$message.success(this.$t('GPS test completed'));
      } catch (error) {
        console.error('GPS test failed:', error);
        this.$message.error(this.$t('GPS test failed'));
      }
    },
    
    async testOpenCELLID() {
      try {
        const response = await this.$http.post('/api/autonomy/opencellid/test');
        this.$message.success(this.$t('OpenCELLID test completed'));
      } catch (error) {
        console.error('OpenCELLID test failed:', error);
        this.$message.error(this.$t('OpenCELLID test failed'));
      }
    }
  }
};
</script>
