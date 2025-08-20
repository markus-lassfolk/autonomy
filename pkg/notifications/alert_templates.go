package notifications

// No imports needed for this file

// initializeDefaultTemplates sets up default alert templates for different failure types
func (cam *ContextualAlertManager) initializeDefaultTemplates() {
	cam.alertTemplates[AlertFailover] = &AlertTemplate{
		Type:          AlertFailover,
		TitleTemplate: "🔄 Failover: {{.from_interface}} → {{.to_interface}}",
		MessageTemplate: `Network failover executed from {{.from_interface}} to {{.to_interface}}.

📊 Performance Impact:
• Previous latency: {{.metrics.latency}}ms
• Packet loss: {{.metrics.loss}}%
• Reason: {{.reason}}

🌍 Location: {{.location.latitude}}, {{.location.longitude}} (±{{.location.accuracy}}m)
⏰ Time: {{.timestamp}}`,
		Priority:        PriorityHigh,
		RequiredContext: []string{"from_interface", "to_interface", "reason"},
		Enrichers: []ContextEnricher{
			{
				Name:     "Weather Impact",
				Type:     "weather",
				Template: "Weather conditions: {{.weather.condition}}, visibility {{.weather.visibility}}km",
				Priority: 2,
			},
			{
				Name:     "Movement Status",
				Type:     "location",
				Template: "Device movement: {{.movement.speed}}m/s, stationary for {{.movement.stationary_time}}",
				Priority: 1,
			},
		},
		Actions: []SuggestedAction{
			{
				Title:       "Monitor new interface",
				Description: "Keep monitoring the new primary interface for stability",
				Command:     "ubus call autonomy metrics '{\"interface\":\"{{.to_interface}}\"}'",
				Priority:    1,
			},
			{
				Title:       "Check backup interfaces",
				Description: "Verify backup interfaces are ready for next failover",
				Command:     "ubus call autonomy members",
				Priority:    2,
			},
		},
	}

	cam.alertTemplates[AlertFailback] = &AlertTemplate{
		Type:          AlertFailback,
		TitleTemplate: "↩️ Failback: {{.to_interface}} restored",
		MessageTemplate: `Network failback completed - {{.to_interface}} is now primary.

📈 Recovery Metrics:
• Current latency: {{.metrics.latency}}ms
• Packet loss: {{.metrics.loss}}%
• Downtime: {{.downtime}}

🌍 Location: {{.location.latitude}}, {{.location.longitude}}
⏰ Time: {{.timestamp}}`,
		Priority:        PriorityNormal,
		RequiredContext: []string{"to_interface", "downtime"},
		Actions: []SuggestedAction{
			{
				Title:       "Verify stability",
				Description: "Monitor the restored interface for 10 minutes to ensure stability",
				Priority:    1,
			},
		},
	}

	cam.alertTemplates[AlertInterfaceDown] = &AlertTemplate{
		Type:          AlertInterfaceDown,
		TitleTemplate: "🔴 Interface Down: {{.interface}}",
		MessageTemplate: `Interface {{.interface}} has gone down.

📉 Last Known Metrics:
• Latency: {{.metrics.latency}}ms
• Loss: {{.metrics.loss}}%
• Signal: {{.signal_strength}}

🔍 Failure Analysis:
• Duration: {{.duration}}
• Cause: {{.cause}}
• Impact: {{.impact}}

🌍 Location: {{.location.latitude}}, {{.location.longitude}}
⏰ Time: {{.timestamp}}`,
		Priority:        PriorityHigh,
		RequiredContext: []string{"interface"},
		Enrichers: []ContextEnricher{
			{
				Name:     "Provider Status",
				Type:     "system",
				Template: "Provider: {{.provider}}, Account status: {{.account_status}}",
				Priority: 1,
			},
			{
				Name:     "Historical Pattern",
				Type:     "historical",
				Template: "Similar failures: {{.historical.frequency}}/day, pattern: {{.historical.pattern_type}}",
				Priority: 2,
			},
		},
		Actions: []SuggestedAction{
			{
				Title:       "Check physical connections",
				Description: "Verify cables and antenna connections are secure",
				Priority:    1,
			},
			{
				Title:       "Restart interface",
				Description: "Attempt to restart the failed interface",
				Command:     "ifdown {{.interface}} && sleep 5 && ifup {{.interface}}",
				Priority:    2,
			},
		},
	}

	cam.alertTemplates[AlertInterfaceUp] = &AlertTemplate{
		Type:          AlertInterfaceUp,
		TitleTemplate: "🟢 Interface Up: {{.interface}}",
		MessageTemplate: `Interface {{.interface}} is now operational.

📊 Current Metrics:
• Latency: {{.metrics.latency}}ms
• Loss: {{.metrics.loss}}%
• Signal: {{.signal_strength}}

⏱️ Recovery Time: {{.recovery_time}}
🌍 Location: {{.location.latitude}}, {{.location.longitude}}
⏰ Time: {{.timestamp}}`,
		Priority:        PriorityNormal,
		RequiredContext: []string{"interface"},
		Actions: []SuggestedAction{
			{
				Title:       "Update routing weights",
				Description: "Consider updating interface priorities based on performance",
				Command:     "ubus call autonomy config.set '{\"interface_priority\":{{.priority}}}'",
				Priority:    2,
			},
		},
	}

	cam.alertTemplates[AlertPredictive] = &AlertTemplate{
		Type:          AlertPredictive,
		TitleTemplate: "⚠️ Predictive Alert: {{.interface}} degrading",
		MessageTemplate: `Predictive analysis indicates {{.interface}} may fail soon.

📉 Degradation Indicators:
• {{.prediction_reason}}
• Confidence: {{.confidence}}%
• Estimated time to failure: {{.time_to_failure}}

📊 Current Metrics:
• Latency: {{.metrics.latency}}ms (trend: {{.latency_trend}})
• Loss: {{.metrics.loss}}% (trend: {{.loss_trend}})

🎯 Recommended Action: {{.recommendation}}

🌍 Location: {{.location.latitude}}, {{.location.longitude}}
⏰ Time: {{.timestamp}}`,
		Priority:        PriorityHigh,
		RequiredContext: []string{"interface", "prediction_reason", "confidence"},
		Enrichers: []ContextEnricher{
			{
				Name:     "Trend Analysis",
				Type:     "metrics",
				Template: "Performance trend over last hour: {{.trend_analysis}}",
				Priority: 1,
			},
		},
		Actions: []SuggestedAction{
			{
				Title:       "Prepare backup interface",
				Description: "Ensure backup interfaces are ready for immediate failover",
				Command:     "ubus call autonomy action '{\"type\":\"prepare_backup\"}'",
				Priority:    1,
			},
			{
				Title:       "Increase monitoring frequency",
				Description: "Switch to high-frequency monitoring for early detection",
				Priority:    2,
			},
		},
	}

	cam.alertTemplates[AlertObstruction] = &AlertTemplate{
		Type:          AlertObstruction,
		TitleTemplate: "🛰️ Starlink Obstruction: {{.severity}} level",
		MessageTemplate: `Starlink obstruction detected affecting connectivity.

🔍 Obstruction Details:
• Severity: {{.severity}}
• Fraction obstructed: {{.fraction_obstructed}}%
• Valid patches: {{.valid_patches}}%
• Time obstructed: {{.time_obstructed}}s

📡 Signal Impact:
• SNR: {{.snr}}dB
• Downlink throughput: {{.downlink_throughput}}Mbps

🌍 Location: {{.location.latitude}}, {{.location.longitude}}
⏰ Time: {{.timestamp}}`,
		Priority:        PriorityNormal,
		RequiredContext: []string{"severity", "fraction_obstructed"},
		Enrichers: []ContextEnricher{
			{
				Name: "Movement Recommendation",
				Type: "location",
				Conditions: []EnricherCondition{
					{Field: "is_stationary", Operator: "equals", Value: true},
				},
				Template: "Device is stationary - consider repositioning for better sky view",
				Priority: 1,
			},
		},
		Actions: []SuggestedAction{
			{
				Title:       "Clear obstruction map",
				Description: "Reset Starlink obstruction map to refresh satellite tracking",
				Command:     "ubus call starlink clear_obstruction_map",
				Priority:    1,
			},
			{
				Title:       "Check physical obstructions",
				Description: "Look for trees, buildings, or other objects blocking satellite view",
				Priority:    2,
			},
		},
	}

	cam.alertTemplates[AlertThermal] = &AlertTemplate{
		Type:          AlertThermal,
		TitleTemplate: "🌡️ Thermal Alert: {{.component}} overheating",
		MessageTemplate: `Thermal management alert for {{.component}}.

🌡️ Temperature Status:
• Current: {{.temperature}}°C
• Threshold: {{.threshold}}°C
• Trend: {{.trend}}

⚡ System Impact:
• CPU usage: {{.system.cpu}}%
• Performance throttling: {{.throttling_active}}
• Estimated shutdown time: {{.shutdown_estimate}}

🌍 Location: {{.location.latitude}}, {{.location.longitude}}
⏰ Time: {{.timestamp}}`,
		Priority:        PriorityHigh,
		RequiredContext: []string{"component", "temperature", "threshold"},
		Enrichers: []ContextEnricher{
			{
				Name:     "Environmental Factors",
				Type:     "weather",
				Template: "Ambient temperature: {{.weather.temperature}}°C, humidity: {{.weather.humidity}}%",
				Priority: 1,
			},
		},
		Actions: []SuggestedAction{
			{
				Title:       "Check cooling system",
				Description: "Verify fans are working and air vents are not blocked",
				Priority:    1,
			},
			{
				Title:       "Reduce system load",
				Description: "Consider reducing non-essential services to lower heat generation",
				Priority:    2,
			},
		},
	}

	cam.alertTemplates[AlertDataLimit] = &AlertTemplate{
		Type:          AlertDataLimit,
		TitleTemplate: "📊 Data Limit Alert: {{.usage_percent}}% used",
		MessageTemplate: `Data usage approaching limit on {{.interface}}.

📈 Usage Statistics:
• Used: {{.data_used}}GB / {{.data_limit}}GB ({{.usage_percent}}%)
• Daily average: {{.daily_average}}GB
• Days remaining: {{.days_remaining}}
• Projected overage: {{.projected_overage}}GB

💰 Cost Impact:
• Overage rate: {{.overage_rate}}/GB
• Estimated additional cost: {{.estimated_cost}}

🌍 Location: {{.location.latitude}}, {{.location.longitude}}
⏰ Time: {{.timestamp}}`,
		Priority:        PriorityNormal,
		RequiredContext: []string{"interface", "usage_percent", "data_used", "data_limit"},
		Actions: []SuggestedAction{
			{
				Title:       "Enable data saving mode",
				Description: "Activate data conservation features to reduce usage",
				Command:     "uci set autonomy.general.data_saving=1 && uci commit",
				Priority:    1,
			},
			{
				Title:       "Review data usage",
				Description: "Check which applications are consuming the most data",
				Priority:    2,
			},
		},
	}

	cam.alertTemplates[AlertSystemHealth] = &AlertTemplate{
		Type:          AlertSystemHealth,
		TitleTemplate: "🏥 System Health Alert: {{.component}}",
		MessageTemplate: `System health issue detected in {{.component}}.

🔍 Health Status:
• Component: {{.component}}
• Status: {{.status}}
• Severity: {{.severity}}
• Description: {{.description}}

📊 System Metrics:
• CPU: {{.system.cpu}}%
• Memory: {{.system.memory}}%
• Temperature: {{.system.temperature}}°C
• Uptime: {{.uptime}}

🌍 Location: {{.location.latitude}}, {{.location.longitude}}
⏰ Time: {{.timestamp}}`,
		Priority:        PriorityHigh,
		RequiredContext: []string{"component", "status", "severity"},
		Actions: []SuggestedAction{
			{
				Title:       "Run system diagnostics",
				Description: "Execute comprehensive system health check",
				Command:     "ubus call autonomy system_diagnostics",
				Priority:    1,
			},
			{
				Title:       "Check system logs",
				Description: "Review system logs for additional error details",
				Command:     "logread | tail -50",
				Priority:    2,
			},
		},
	}

	cam.alertTemplates[AlertConnectivityIssue] = &AlertTemplate{
		Type:          AlertConnectivityIssue,
		TitleTemplate: "🌐 Connectivity Issue: {{.interface}}",
		MessageTemplate: `Connectivity problems detected on {{.interface}}.

📡 Connection Status:
• Interface: {{.interface}}
• Issue type: {{.issue_type}}
• Severity: {{.severity}}
• Duration: {{.duration}}

📊 Performance Impact:
• Latency: {{.metrics.latency}}ms (normal: {{.baseline_latency}}ms)
• Packet loss: {{.metrics.loss}}% (normal: {{.baseline_loss}}%)
• Throughput: {{.throughput}}Mbps (normal: {{.baseline_throughput}}Mbps)

🔍 Troubleshooting:
• DNS resolution: {{.dns_status}}
• Gateway reachability: {{.gateway_status}}
• External connectivity: {{.external_status}}

🌍 Location: {{.location.latitude}}, {{.location.longitude}}
⏰ Time: {{.timestamp}}`,
		Priority:        PriorityNormal,
		RequiredContext: []string{"interface", "issue_type", "severity"},
		Actions: []SuggestedAction{
			{
				Title:       "Run connectivity test",
				Description: "Execute comprehensive connectivity diagnostics",
				Command:     "ubus call autonomy connectivity_test '{\"interface\":\"{{.interface}}\"}'",
				Priority:    1,
			},
			{
				Title:       "Reset network interface",
				Description: "Restart the network interface to resolve temporary issues",
				Command:     "ifdown {{.interface}} && sleep 5 && ifup {{.interface}}",
				Priority:    2,
			},
		},
	}
}

// initializeDefaultContextRules sets up default context rules for intelligent behavior
func (cam *ContextualAlertManager) initializeDefaultContextRules() {
	cam.contextRules = []ContextRule{
		{
			ID:   "quiet_hours_low_priority",
			Name: "Suppress low priority alerts during quiet hours",
			Conditions: []RuleCondition{
				{Type: "time", Field: "hour", Operator: "gte", Value: 22},
				{Type: "time", Field: "hour", Operator: "lt", Value: 8},
			},
			Actions: []ContextAction{
				{Type: "modify_priority", Parameters: map[string]interface{}{"priority": PriorityLowest}},
			},
			Priority: 1,
			Enabled:  true,
		},
		{
			ID:   "high_temperature_escalation",
			Name: "Escalate thermal alerts when temperature is critical",
			Conditions: []RuleCondition{
				{Type: "alert_type", Field: "", Operator: "equals", Value: "thermal"},
				{Type: "system", Field: "temperature", Operator: "gt", Value: 80.0},
			},
			Actions: []ContextAction{
				{Type: "modify_priority", Parameters: map[string]interface{}{"priority": PriorityEmergency}},
				{Type: "add_context", Parameters: map[string]interface{}{"key": "critical_temperature", "value": true}},
			},
			Priority: 1,
			Enabled:  true,
		},
		{
			ID:   "mobile_movement_context",
			Name: "Add movement context for mobile deployments",
			Conditions: []RuleCondition{
				{Type: "location", Field: "is_stationary", Operator: "equals", Value: false},
			},
			Actions: []ContextAction{
				{Type: "add_context", Parameters: map[string]interface{}{"key": "mobile_deployment", "value": true}},
			},
			Priority: 2,
			Enabled:  true,
		},
		{
			ID:   "weekend_low_priority_suppression",
			Name: "Reduce priority for non-critical alerts on weekends",
			Conditions: []RuleCondition{
				{Type: "time", Field: "weekday", Operator: "equals", Value: "Saturday"},
			},
			Actions: []ContextAction{
				{Type: "modify_priority", Parameters: map[string]interface{}{"priority": PriorityLow}},
			},
			Priority: 2,
			Enabled:  false, // Disabled by default
		},
		{
			ID:   "high_loss_emergency_escalation",
			Name: "Escalate alerts when packet loss is extremely high",
			Conditions: []RuleCondition{
				{Type: "metrics", Field: "loss_percent", Operator: "gt", Value: 50.0},
			},
			Actions: []ContextAction{
				{Type: "modify_priority", Parameters: map[string]interface{}{"priority": PriorityEmergency}},
				{Type: "add_context", Parameters: map[string]interface{}{"key": "critical_loss", "value": true}},
			},
			Priority: 1,
			Enabled:  true,
		},
		{
			ID:   "maintenance_mode_suppression",
			Name: "Suppress non-emergency alerts during maintenance",
			Conditions: []RuleCondition{
				{Type: "system", Field: "in_maintenance", Operator: "equals", Value: true},
			},
			Actions: []ContextAction{
				{Type: "modify_priority", Parameters: map[string]interface{}{"priority": PriorityLowest}},
				{Type: "add_context", Parameters: map[string]interface{}{"key": "maintenance_active", "value": true}},
			},
			Priority: 1,
			Enabled:  true,
		},
	}
}
