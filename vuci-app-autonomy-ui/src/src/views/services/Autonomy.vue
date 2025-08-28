<template>
  <div>
    <tlt-card :title="$t('Autonomy Overview')">
      <div class="row">
        <div class="col-md-6">
          <strong>{{ $t('Status') }}:</strong>
          <span :class="{'text-success': status.running, 'text-danger': !status.running}">
            {{ status.running ? $t('Running') : $t('Stopped') }}
          </span>
        </div>
        <div class="col-md-6">
          <strong>{{ $t('Uptime') }}:</strong> {{ status.uptime || 'N/A' }}
        </div>
      </div>
      <div class="row mt-2">
        <div class="col-12">
          <strong>{{ $t('Last Check') }}:</strong> {{ status.last_check || 'N/A' }}
        </div>
      </div>
      <div class="row mt-2" v-if="status.issues && status.issues.length > 0">
        <div class="col-12">
          <strong>{{ $t('Issues') }}:</strong>
          <ul class="list-unstyled">
            <li v-for="issue in status.issues" :key="issue" class="text-danger">{{ issue }}</li>
          </ul>
        </div>
      </div>
      <div class="row mt-3">
        <div class="col-12">
          <tlt-button @click="loadStatus" class="me-2">
            {{ $t('Refresh') }}
          </tlt-button>
          <tlt-button @click="restartService" variant="warning">
            {{ $t('Restart Service') }}
          </tlt-button>
        </div>
      </div>
    </tlt-card>

    <vuci-form
      v-slot="{ uciData }"
      v-model="cAutonomyForm"
      config="autonomy"
      async-load
    >
      <vuci-typed-section
        type="autonomy"
        data-key="autonomy"
        :title="$t('Configuration')"
        :columns="configColumns"
        :edit-form="editModal"
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
          <vuci-form-item-dummy :uci-section="s" name="check_interval" />
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
  </div>
</template>

<script>
import { markRaw } from "vue";

export default {
  data() {
    return {
      status: {
        running: false,
        uptime: '',
        last_check: '',
        issues: []
      },
      cAutonomyForm: {},
      editModal: null,
      configColumns: [
        {
          name: "id",
          label: this.$t("Name"),
        },
        {
          name: "enabled",
          label: this.$t("Enabled"),
        },
        {
          name: "check_interval",
          label: this.$t("Check Interval"),
        },
        {
          name: "starlink_enabled",
          label: this.$t("Starlink Enabled"),
        },
        {
          name: "cellular_enabled",
          label: this.$t("Cellular Enabled"),
        },
        {
          name: "gps_enabled",
          label: this.$t("GPS Enabled"),
        },
      ],
    };
  },
  mounted() {
    this.loadStatus();
  },
  methods: {
    async loadStatus() {
      this.$spin();
      try {
        const response = await this.$axios.get("/api/autonomy_f/status");
        this.status = response.data;
      } catch (error) {
        console.error('Failed to load status:', error);
        this.status = {
          running: false,
          uptime: '',
          last_check: '',
          issues: ['Failed to load status']
        };
      } finally {
        this.$spin(false);
      }
    },
    async restartService() {
      this.$spin();
      try {
        const response = await this.$axios.post("/api/autonomy_f/actions/restart");
        this.$message.success(this.$t('Service restarted successfully'));
        this.loadStatus();
      } catch (error) {
        console.error('Failed to restart service:', error);
        this.$message.error(this.$t('Failed to restart service'));
      } finally {
        this.$spin(false);
      }
    },
  },
};
</script>
