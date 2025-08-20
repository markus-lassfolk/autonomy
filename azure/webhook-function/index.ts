import { AzureFunction, Context, HttpRequest } from "@azure/functions";
import * as crypto from 'crypto';
import * as https from 'https';

// Configuration
const WEBHOOK_SECRET = process.env.WEBHOOK_SECRET || '';
const GITHUB_TOKEN = process.env.GITHUB_TOKEN || '';
const SUPPORTED_VERSIONS = (process.env.SUPPORTED_VERSIONS || 'RUTX_R_00.07.17,RUTX_R_00.07.18,RUTX_R_00.08.00').split(',');
const MIN_SEVERITY = process.env.MIN_SEVERITY || 'warn';
const COPILOT_ENABLED = process.env.COPILOT_ENABLED === 'true';
const AUTO_ASSIGN = process.env.AUTO_ASSIGN === 'true';

// Severity levels
const SEVERITY_LEVELS: { [key: string]: number } = {
  'info': 1,
  'warn': 2,
  'critical': 3
};

// Configuration error keywords
const CONFIG_ERRORS = [
  'configuration_error',
  'user_misconfiguration',
  'network_setup_error',
  'config_error'
];

// System-level issues
const SYSTEM_ISSUES = [
  'daemon_down',
  'daemon_hung',
  'crash_loop',
  'system_degraded'
];

interface WebhookPayload {
  device_id: string;
  fw: string;
  severity: string;
  scenario: string;
  note: string;
  overlay_pct: number;
  mem_avail_mb: number;
  load1: number;
  ubus_ok: boolean;
  actions: string[];
  ts: number;
}

interface GitHubIssue {
  title: string;
  body: string;
  labels: string[];
}

function validateHMAC(payload: string, signature: string): boolean {
  if (!signature || !signature.startsWith('sha256=')) {
    return false;
  }
  
  const expectedSig = crypto
    .createHmac('sha256', WEBHOOK_SECRET)
    .update(payload)
    .digest('hex');
  
  return signature === `sha256=${expectedSig}`;
}

function isSupportedVersion(fw: string): boolean {
  return SUPPORTED_VERSIONS.some(version => fw.startsWith(version));
}

function isCodeIssue(scenario: string, note: string): boolean {
  // Filter out configuration errors
  const noteLower = note.toLowerCase();
  if (CONFIG_ERRORS.some(error => noteLower.includes(error))) {
    return false;
  }
  
  // Focus on system-level issues
  return SYSTEM_ISSUES.includes(scenario);
}

function generateIssueKey(payload: WebhookPayload): string {
  // Create a stable, privacy-safe issue key
  const fwMajorMinor = payload.fw.split('.').slice(0, 2).join('.');
  const keyData = `${payload.device_id}|${payload.scenario}|${fwMajorMinor}`;
  return crypto.createHash('sha256').update(keyData).digest('hex').slice(0, 12);
}

function searchExistingIssue(issueKey: string): Promise<any> {
  return new Promise((resolve, reject) => {
    const query = `repo:markus-lassfolk/rutos-starlink-failover state:open label:autonomy-alert in:body "Issue Key: ${issueKey}"`;
    const encodedQuery = encodeURIComponent(query);
    
    const options = {
      hostname: 'api.github.com',
      port: 443,
      path: `/search/issues?q=${encodedQuery}`,
      method: 'GET',
      headers: {
        'Authorization': `token ${GITHUB_TOKEN}`,
        'User-Agent': 'autonomy-Webhook-Receiver/1.0',
        'Accept': 'application/vnd.github.v3+json'
      }
    };

    const req = https.request(options, (res) => {
      let data = '';
      
      res.on('data', (chunk) => {
        data += chunk;
      });
      
      res.on('end', () => {
        if (res.statusCode && res.statusCode >= 200 && res.statusCode < 300) {
          const searchResult = JSON.parse(data);
          if (searchResult.items && searchResult.items.length > 0) {
            resolve(searchResult.items[0]); // Return first matching issue
          } else {
            resolve(null); // No existing issue found
          }
        } else {
          console.error(`❌ Failed to search issues: ${res.statusCode} ${data}`);
          reject(new Error(`HTTP ${res.statusCode}: ${data}`));
        }
      });
    });

    req.on('error', (err) => {
      console.error(`❌ Search request error: ${err.message}`);
      reject(err);
    });

    req.end();
  });
}

function createGitHubIssue(payload: WebhookPayload): Promise<any> {
  const issueKey = generateIssueKey(payload);
  const fwMajorMinor = payload.fw.split('.').slice(0, 2).join('.');
  
  // Use generic, non-identifying title
  const title = `[autonomy] ${payload.scenario} on RUTX fw ${fwMajorMinor}`;
  
  const body = `## autonomy Alert Report

**Firmware**: ${payload.fw}
**Severity**: ${payload.severity}
**Scenario**: ${payload.scenario}
**Timestamp**: ${new Date(payload.ts * 1000).toISOString()}

### System Status
- **Overlay Usage**: ${payload.overlay_pct}%
- **Available Memory**: ${payload.mem_avail_mb} MB
- **Load Average**: ${payload.load1}
- **Ubus Status**: ${payload.ubus_ok}

### Actions Taken
${JSON.stringify(payload.actions)}

### Description
${payload.note}

### Diagnostic Information
This issue was automatically created by the autonomy webhook receiver.
- **Issue Key**: ${issueKey}
- **Alert ID**: ${Date.now()}
- **Webhook Receiver**: Azure Function

### Next Steps
1. Review system logs for additional context
2. Check if this is a known issue
3. Verify system configuration
4. Consider firmware update if applicable

---
*This issue was automatically generated by the autonomy monitoring system.*`;

  // Determine labels (privacy-safe)
  const labels = ['autonomy-alert', 'auto-generated'];
  
  // Add severity label
  switch (payload.severity) {
    case 'critical':
      labels.push('severity-critical');
      break;
    case 'warn':
      labels.push('severity-warning');
      break;
    case 'info':
      labels.push('severity-info');
      break;
  }
  
  // Add component label
  switch (payload.scenario) {
    case 'daemon_down':
    case 'daemon_hung':
    case 'crash_loop':
      labels.push('component-daemon');
      break;
    case 'system_degraded':
    case 'post_reboot':
      labels.push('component-system');
      break;
    case 'slow':
      labels.push('component-performance');
      break;
  }
  
  // Add firmware version label (privacy-safe)
  labels.push(`fw-${fwMajorMinor}`);
  
  // Add device hash label for fleet grouping (optional)
  if (payload.device_id && payload.device_id.length === 12) {
    labels.push(`device-hash-${payload.device_id}`);
  }

  const issueData: GitHubIssue = {
    title,
    body,
    labels
  };

  return new Promise((resolve, reject) => {
    const postData = JSON.stringify(issueData);
    
    const options = {
      hostname: 'api.github.com',
      port: 443,
      path: '/repos/markus-lassfolk/rutos-starlink-failover/issues',
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Content-Length': Buffer.byteLength(postData),
        'Authorization': `token ${GITHUB_TOKEN}`,
        'User-Agent': 'autonomy-Webhook-Receiver/1.0'
      }
    };

    const req = https.request(options, (res) => {
      let data = '';
      
      res.on('data', (chunk) => {
        data += chunk;
      });
      
      res.on('end', () => {
        if (res.statusCode && res.statusCode >= 200 && res.statusCode < 300) {
          const issue = JSON.parse(data);
          console.log(`✅ Created issue #${issue.number}`);
          
          // Add additional context comment
          addContextComment(issue.number, payload);
          
          resolve(issue);
        } else {
          console.error(`❌ Failed to create issue: ${res.statusCode} ${data}`);
          reject(new Error(`HTTP ${res.statusCode}: ${data}`));
        }
      });
    });

    req.on('error', (err) => {
      console.error(`❌ Request error: ${err.message}`);
      reject(err);
    });

    req.write(postData);
    req.end();
  });
}

function addContextComment(issueNumber: number, payload: WebhookPayload): void {
  const commentBody = `## Additional Context

**Alert Details**:
- **Firmware Version**: ${payload.fw}
- **Alert Severity**: ${payload.severity}
- **Issue Scenario**: ${payload.scenario}

**System Metrics**:
- **Overlay Usage**: ${payload.overlay_pct}%
- **Available Memory**: ${payload.mem_avail_mb} MB
- **Load Average**: ${payload.load1}
- **Ubus Status**: ${payload.ubus_ok}

**Actions Performed**:
${JSON.stringify(payload.actions)}

**Description**:
${payload.note}

---
*This comment was automatically added by the webhook receiver.*`;

  const postData = JSON.stringify({ body: commentBody });
  
  const options = {
    hostname: 'api.github.com',
    port: 443,
    path: `/repos/markus-lassfolk/rutos-starlink-failover/issues/${issueNumber}/comments`,
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      'Content-Length': Buffer.byteLength(postData),
      'Authorization': `token ${GITHUB_TOKEN}`,
      'User-Agent': 'autonomy-Webhook-Receiver/1.0'
    }
  };

  const req = https.request(options, (res) => {
    if (res.statusCode && res.statusCode >= 200 && res.statusCode < 300) {
      console.log(`✅ Added context comment to issue #${issueNumber}`);
    } else {
      console.error(`⚠️ Failed to add comment to issue #${issueNumber}: ${res.statusCode}`);
    }
  });

  req.on('error', (err) => {
    console.error(`❌ Comment request error: ${err.message}`);
  });

  req.write(postData);
  req.end();
}

function updateIssueLabels(issueNumber: number, newLabels: string[]): void {
  const postData = JSON.stringify({ labels: newLabels });
  
  const options = {
    hostname: 'api.github.com',
    port: 443,
    path: `/repos/markus-lassfolk/rutos-starlink-failover/issues/${issueNumber}`,
    method: 'PATCH',
    headers: {
      'Content-Type': 'application/json',
      'Content-Length': Buffer.byteLength(postData),
      'Authorization': `token ${GITHUB_TOKEN}`,
      'User-Agent': 'autonomy-Webhook-Receiver/1.0'
    }
  };

  const req = https.request(options, (res) => {
    if (res.statusCode && res.statusCode >= 200 && res.statusCode < 300) {
      console.log(`✅ Updated labels for issue #${issueNumber}`);
    } else {
      console.error(`⚠️ Failed to update labels for issue #${issueNumber}: ${res.statusCode}`);
    }
  });

  req.on('error', (err) => {
    console.error(`❌ Update labels request error: ${err.message}`);
  });

  req.write(postData);
  req.end();
}

async function processWebhook(payload: WebhookPayload, signature: string): Promise<{ success: boolean; reason?: string; issue?: any }> {
  console.log('Received payload:', JSON.stringify(payload, null, 2));
  
  // Validate HMAC signature
  if (!validateHMAC(JSON.stringify(payload), signature)) {
    console.error('❌ Invalid HMAC signature');
    return { success: false, reason: 'invalid_signature' };
  }
  
  // Version filtering
  if (!isSupportedVersion(payload.fw)) {
    console.log(`⚠️ Skipping issue creation - unsupported firmware version: ${payload.fw}`);
    return { success: false, reason: 'unsupported_version' };
  }
  
  // Severity filtering
  const severityLevel = SEVERITY_LEVELS[payload.severity] || 0;
  const minSeverityLevel = SEVERITY_LEVELS[MIN_SEVERITY] || 0;
  
  if (severityLevel < minSeverityLevel) {
    console.log(`⚠️ Skipping issue creation - severity below threshold: ${payload.severity}`);
    return { success: false, reason: 'severity_filtered' };
  }
  
  // Configuration vs code issue filtering
  if (!isCodeIssue(payload.scenario, payload.note)) {
    console.log(`⚠️ Skipping issue creation - not a code issue: ${payload.scenario}`);
    return { success: false, reason: 'not_code_issue' };
  }
  
  // Check for existing issue with same key
  const issueKey = generateIssueKey(payload);
  console.log(`🔍 Searching for existing issue with key: ${issueKey}`);
  
  try {
    const existingIssue = await searchExistingIssue(issueKey);
    
    if (existingIssue) {
      console.log(`📝 Found existing issue #${existingIssue.number}, adding comment`);
      
      // Add context comment to existing issue
      addContextComment(existingIssue.number, payload);
      
      // Update labels if severity increased
      const currentSeverity = SEVERITY_LEVELS[payload.severity] || 0;
      const existingSeverity = SEVERITY_LEVELS[existingIssue.labels.find((l: any) => l.name.startsWith('severity-'))?.name.replace('severity-', '')] || 0;
      
      if (currentSeverity > existingSeverity) {
        console.log(`⚠️ Severity increased, updating labels for issue #${existingIssue.number}`);
        const newLabels = existingIssue.labels.map((l: any) => l.name).filter((l: string) => !l.startsWith('severity-'));
        newLabels.push(`severity-${payload.severity}`);
        updateIssueLabels(existingIssue.number, newLabels);
      }
      
      return { success: true, issue: existingIssue, action: 'commented' };
    } else {
      console.log(`🆕 No existing issue found, creating new issue`);
      
      // Create new GitHub issue
      const issue = await createGitHubIssue(payload);
      console.log(`✅ Successfully created issue #${issue.number}`);
      return { success: true, issue, action: 'created' };
    }
  } catch (error) {
    console.error(`❌ Failed to process issue: ${error}`);
    return { success: false, reason: 'github_error', error };
  }
}

const httpTrigger: AzureFunction = async function (context: Context, req: HttpRequest): Promise<void> {
  context.log('HTTP trigger function processed a request.');

  try {
    // Get payload and signature from request
    const payload: WebhookPayload = req.body;
    const signature = req.headers['x-starwatch-signature'] || '';

    if (!payload) {
      context.res = {
        status: 400,
        body: { error: 'No payload provided' }
      };
      return;
    }

    // Process the webhook
    const result = await processWebhook(payload, signature);

    if (result.success) {
      context.res = {
        status: 200,
        body: { 
          success: true, 
          message: 'Issue created successfully',
          issue_number: result.issue?.number 
        }
      };
    } else {
      context.res = {
        status: 200,
        body: { 
          success: false, 
          reason: result.reason,
          message: `Webhook skipped: ${result.reason}` 
        }
      };
    }
  } catch (error) {
    context.log.error('Error processing webhook:', error);
    context.res = {
      status: 500,
      body: { error: 'Internal server error' }
    };
  }
};

export default httpTrigger;
