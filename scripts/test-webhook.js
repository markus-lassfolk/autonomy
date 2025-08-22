#!/usr/bin/env node

const crypto = require('crypto');
const https = require('https');

// Test configuration
const WEBHOOK_URL = process.env.WEBHOOK_URL || 'http://localhost:7071/api/webhook';
const WEBHOOK_SECRET = process.env.WEBHOOK_SECRET || 'test-secret';

// Test payloads
const testPayloads = [
  {
    name: "Valid Critical Alert",
    payload: {
      device_id: "rutx50-test-01",
      fw: "RUTX_R_00.07.17",
      severity: "critical",
      scenario: "daemon_down",
      note: "Daemon process not responding",
      overlay_pct: 95,
      mem_avail_mb: 25,
      load1: 4.2,
      ubus_ok: false,
      actions: ["restart", "hold_down"],
      ts: Math.floor(Date.now() / 1000)
    },
    shouldCreate: true
  },
  {
    name: "Valid Warning Alert",
    payload: {
      device_id: "rutx50-test-02",
      fw: "RUTX_R_00.07.18",
      severity: "warn",
      scenario: "system_degraded",
      note: "High memory usage detected",
      overlay_pct: 85,
      mem_avail_mb: 50,
      load1: 2.1,
      ubus_ok: true,
      actions: ["log_prune"],
      ts: Math.floor(Date.now() / 1000)
    },
    shouldCreate: true
  },
  {
    name: "Unsupported Version",
    payload: {
      device_id: "rutx50-test-03",
      fw: "RUTX_R_00.06.99",
      severity: "critical",
      scenario: "daemon_down",
      note: "Daemon process not responding",
      overlay_pct: 90,
      mem_avail_mb: 30,
      load1: 3.5,
      ubus_ok: false,
      actions: ["restart"],
      ts: Math.floor(Date.now() / 1000)
    },
    shouldCreate: false
  },
  {
    name: "Configuration Error",
    payload: {
      device_id: "rutx50-test-04",
      fw: "RUTX_R_00.07.17",
      severity: "warn",
      scenario: "slow",
      note: "Configuration error detected in network setup",
      overlay_pct: 70,
      mem_avail_mb: 100,
      load1: 1.5,
      ubus_ok: true,
      actions: [],
      ts: Math.floor(Date.now() / 1000)
    },
    shouldCreate: false
  },
  {
    name: "Info Level (Below Threshold)",
    payload: {
      device_id: "rutx50-test-05",
      fw: "RUTX_R_00.07.17",
      severity: "info",
      scenario: "post_reboot",
      note: "System rebooted successfully",
      overlay_pct: 60,
      mem_avail_mb: 150,
      load1: 0.8,
      ubus_ok: true,
      actions: [],
      ts: Math.floor(Date.now() / 1000)
    },
    shouldCreate: false
  }
];

function generateHMAC(payload, secret) {
  return crypto
    .createHmac('sha256', secret)
    .update(JSON.stringify(payload))
    .digest('hex');
}

function sendWebhook(url, payload, signature) {
  return new Promise((resolve, reject) => {
    const postData = JSON.stringify(payload);
    
    const urlObj = new URL(url);
    const options = {
      hostname: urlObj.hostname,
      port: urlObj.port || (urlObj.protocol === 'https:' ? 443 : 80),
      path: urlObj.pathname,
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Content-Length': Buffer.byteLength(postData),
        'X-Starwatch-Signature': `sha256=${signature}`
      }
    };

    const req = https.request(options, (res) => {
      let data = '';
      
      res.on('data', (chunk) => {
        data += chunk;
      });
      
      res.on('end', () => {
        try {
          const response = JSON.parse(data);
          resolve({
            statusCode: res.statusCode,
            response: response
          });
        } catch (error) {
          resolve({
            statusCode: res.statusCode,
            response: data
          });
        }
      });
    });

    req.on('error', (err) => {
      reject(err);
    });

    req.write(postData);
    req.end();
  });
}

async function runTests() {
  console.log('🧪 Starting webhook receiver tests...\n');
  
  let passed = 0;
  let failed = 0;
  
  for (const test of testPayloads) {
    console.log(`📋 Testing: ${test.name}`);
    console.log(`   Payload: ${JSON.stringify(test.payload, null, 2)}`);
    
    try {
      const signature = generateHMAC(test.payload, WEBHOOK_SECRET);
      const result = await sendWebhook(WEBHOOK_URL, test.payload, signature);
      
      console.log(`   Status: ${result.statusCode}`);
      console.log(`   Response: ${JSON.stringify(result.response, null, 2)}`);
      
      // Check if the result matches expectations
      const success = result.response.success;
      const expectedSuccess = test.shouldCreate;
      
      if (success === expectedSuccess) {
        console.log(`   ✅ PASS - Expected ${expectedSuccess}, got ${success}`);
        passed++;
      } else {
        console.log(`   ❌ FAIL - Expected ${expectedSuccess}, got ${success}`);
        failed++;
      }
      
    } catch (error) {
      console.log(`   ❌ ERROR - ${error.message}`);
      failed++;
    }
    
    console.log('');
  }
  
  console.log('📊 Test Results:');
  console.log(`   ✅ Passed: ${passed}`);
  console.log(`   ❌ Failed: ${failed}`);
  console.log(`   📈 Success Rate: ${((passed / (passed + failed)) * 100).toFixed(1)}%`);
  
  if (failed > 0) {
    process.exit(1);
  } else {
    console.log('\n🎉 All tests passed!');
  }
}

// Run tests if this script is executed directly
if (require.main === module) {
  runTests().catch(error => {
    console.error('❌ Test execution failed:', error);
    process.exit(1);
  });
}

module.exports = { runTests, testPayloads, generateHMAC, sendWebhook };
