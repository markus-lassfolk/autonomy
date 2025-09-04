(function(){
  function readCookie(name){
    const m = document.cookie.match(new RegExp('(^| )'+name+'=([^;]+)')); return m?decodeURIComponent(m[2]):null;
  }
  
  async function ubusCall(obj, method, params){
    // Try to reuse LuCI/VuCI session token from cookie 'sysauth'
    const sid = readCookie('sysauth') || readCookie('ubus_sessionid') || null;
    const payload = {
      jsonrpc: "2.0",
      id: 1,
      method: "call",
      params: [ sid, obj, method, params || {} ]
    };
    const res = await fetch("/ubus", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(payload),
      credentials: "same-origin"
    });
    const data = await res.json();
    if(data.error) throw data.error;
    return data.result && data.result[1] ? data.result[1] : data;
  }
  
  function createButton(text, onClick) {
    const btn = document.createElement('button');
    btn.textContent = text;
    btn.onclick = onClick;
    btn.style.margin = '5px';
    btn.style.padding = '8px 16px';
    return btn;
  }
  
  function createStatusDisplay() {
    const div = document.createElement('div');
    div.style.border = '1px solid #ccc';
    div.style.padding = '10px';
    div.style.margin = '10px 0';
    div.style.backgroundColor = '#f9f9f9';
    return div;
  }
  
  async function main(){
    const root = document.getElementById('autonomy-ui') || document.body;
    
    // Create status display
    const statusDiv = createStatusDisplay();
    statusDiv.innerHTML = '<h3>Status</h3><p>Loading...</p>';
    root.appendChild(statusDiv);
    
    // Create control buttons
    const controlsDiv = document.createElement('div');
    controlsDiv.innerHTML = '<h3>Controls</h3>';
    root.appendChild(controlsDiv);
    
    // Status button
    controlsDiv.appendChild(createButton('Refresh Status', async () => {
      try {
        statusDiv.innerHTML = '<h3>Status</h3><p>Loading...</p>';
        const status = await ubusCall("autonomy", "status", {});
        statusDiv.innerHTML = '<h3>Status</h3><pre>' + JSON.stringify(status, null, 2) + '</pre>';
      } catch(e) {
        statusDiv.innerHTML = '<h3>Status</h3><p style="color: red;">Error: ' + e.message + '</p>';
      }
    }));
    
    // Start button
    controlsDiv.appendChild(createButton('Start Service', async () => {
      try {
        const result = await ubusCall("autonomy", "start", {});
        alert("Start result: " + JSON.stringify(result));
        // Refresh status after start
        const status = await ubusCall("autonomy", "status", {});
        statusDiv.innerHTML = '<h3>Status</h3><pre>' + JSON.stringify(status, null, 2) + '</pre>';
      } catch(e) {
        alert("Error starting service: " + e.message);
      }
    }));
    
    // Stop button
    controlsDiv.appendChild(createButton('Stop Service', async () => {
      try {
        const result = await ubusCall("autonomy", "stop", {});
        alert("Stop result: " + JSON.stringify(result));
        // Refresh status after stop
        const status = await ubusCall("autonomy", "status", {});
        statusDiv.innerHTML = '<h3>Status</h3><pre>' + JSON.stringify(status, null, 2) + '</pre>';
      } catch(e) {
        alert("Error stopping service: " + e.message);
      }
    }));
    
    // Restart button
    controlsDiv.appendChild(createButton('Restart Service', async () => {
      try {
        const result = await ubusCall("autonomy", "restart", {});
        alert("Restart result: " + JSON.stringify(result));
        // Refresh status after restart
        const status = await ubusCall("autonomy", "status", {});
        statusDiv.innerHTML = '<h3>Status</h3><pre>' + JSON.stringify(status, null, 2) + '</pre>';
      } catch(e) {
        alert("Error restarting service: " + e.message);
      }
    }));
    
    // Health check button
    controlsDiv.appendChild(createButton('Run Health Check', async () => {
      try {
        const result = await ubusCall("autonomy", "health", {});
        alert("Health check result: " + JSON.stringify(result));
        // Refresh status after health check
        const status = await ubusCall("autonomy", "status", {});
        statusDiv.innerHTML = '<h3>Status</h3><pre>' + JSON.stringify(status, null, 2) + '</pre>';
      } catch(e) {
        alert("Error running health check: " + e.message);
      }
    }));
    
    // Load initial status
    try {
      const status = await ubusCall("autonomy", "status", {});
      statusDiv.innerHTML = '<h3>Status</h3><pre>' + JSON.stringify(status, null, 2) + '</pre>';
    } catch(e) {
      statusDiv.innerHTML = '<h3>Status</h3><p style="color: red;">Error loading status: ' + e.message + '</p>';
    }
  }
  
  document.addEventListener('DOMContentLoaded', main);
})();
