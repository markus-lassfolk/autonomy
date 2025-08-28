<template>
  <div>
    <tlt-card :title="$t('Autonomy Configuration')">
      <vuci-form
        v-slot="{ uciData }"
        v-model="cAutonomyForm"
        config="autonomy"
        async-load
      >
        <vuci-typed-section
          type="autonomy"
          data-key="autonomy"
          :title="$t('Autonomy Configuration')"
          :columns="configColumns"
          :endpoints="[{ endpoint: 'autonomy_c/config' }]"
          :uci-data="uciData"
        >
          <template #id="{ s }">
            <vuci-form-item-dummy :uci-section="s" name="id" />
          </template>
          <template #enabled="{ s }">
            <vuci-form-item-switch :uci-section="s" name="enabled" />
          </template>
          <template #check_interval="{ s }">
            <vuci-form-item-input :uci-section="s" name="check_interval" />
          </template>
          <template #starlink_enabled="{ s }">
            <vuci-form-item-switch :uci-section="s" name="starlink_enabled" />
          </template>
          <template #cellular_enabled="{ s }">
            <vuci-form-item-switch :uci-section="s" name="cellular_enabled" />
          </template>
          <template #gps_enabled="{ s }">
            <vuci-form-item-switch :uci-section="s" name="gps_enabled" />
          </template>
          <template #log_level="{ s }">
            <vuci-form-item-select :uci-section="s" name="log_level" :options="logLevelOptions" />
          </template>
          <template #failover_threshold="{ s }">
            <vuci-form-item-input :uci-section="s" name="failover_threshold" />
          </template>
          <template #addForm="{ addModel }">
            <tlt-form-item-input
              v-model="addModel.id"
              :label="$t('Name')"
              rules="uciname"
              maxlength="32"
              prop="id"
              required
            />
          </template>
        </vuci-typed-section>
      </vuci-form>
    </tlt-card>
  </div>
</template>

<script>
export default {
  data() {
    return {
      cAutonomyForm: {},
      configColumns: [
        {
          name: 'id',
          label: this.$t('Name')
        },
        {
          name: 'enabled',
          label: this.$t('Enabled')
        },
        {
          name: 'check_interval',
          label: this.$t('Check Interval')
        },
        {
          name: 'starlink_enabled',
          label: this.$t('Starlink')
        },
        {
          name: 'cellular_enabled',
          label: this.$t('Cellular')
        },
        {
          name: 'gps_enabled',
          label: this.$t('GPS')
        },
        {
          name: 'log_level',
          label: this.$t('Log Level')
        },
        {
          name: 'failover_threshold',
          label: this.$t('Failover Threshold')
        }
      ],
      logLevelOptions: [
        { value: 'debug', label: this.$t('Debug') },
        { value: 'info', label: this.$t('Info') },
        { value: 'warn', label: this.$t('Warning') },
        { value: 'error', label: this.$t('Error') }
      ]
    }
  }
}
</script>





