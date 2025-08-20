# Enhanced System Maintenance Script for RUTOS
# This script includes ubus health monitoring and troubleshooting

param(
    [switch]$CheckUbus,
    [switch]$FixUbus,
    [switch]$FullMaintenance,
    [switch]$TestOnly
)

$SSH_KEY = $env:SSH_KEY_PATH ?? "C:\path\to\your\ssh\key"
$RUTOS_HOST = $env:RUTOS_HOST ?? "192.168.1.1"

function Test-UbusHealth {
    Write-Host "🔍 Checking ubus health..." -ForegroundColor Cyan
    
    $ubusTest = ssh -i $SSH_KEY root@$RUTOS_HOST "ubus list 2>&1 | head -5"
    if ($LASTEXITCODE -eq 0 -and $ubusTest -notmatch "Failed to connect") {
        Write-Host "✅ ubus is responding normally" -ForegroundColor Green
        return $true
    } else {
        Write-Host "❌ ubus is not responding" -ForegroundColor Red
        return $false
    }
}

function Fix-UbusIssues {
    Write-Host "🔧 Attempting to fix ubus issues..." -ForegroundColor Yellow
    
    # Check if rpcd init script exists
    $rpcdExists = ssh -i $SSH_KEY root@$RUTOS_HOST "test -f /etc/init.d/rpcd && echo 'exists' || echo 'missing'"
    
    if ($rpcdExists -eq "exists") {
        Write-Host "📋 Restarting rpcd service..." -ForegroundColor Cyan
        ssh -i $SSH_KEY root@$RUTOS_HOST "/etc/init.d/rpcd restart"
        
        # Wait for service to stabilize
        Start-Sleep -Seconds 5
        
        # Test ubus again
        if (Test-UbusHealth) {
            Write-Host "✅ ubus fixed by restarting rpcd" -ForegroundColor Green
            return $true
        }
    }
    
    # Fallback: manual ubus restart
    Write-Host "🔄 Attempting manual ubus restart..." -ForegroundColor Yellow
    ssh -i $SSH_KEY root@$RUTOS_HOST "killall ubusd 2>/dev/null; sleep 2; /sbin/ubusd &"
    
    Start-Sleep -Seconds 3
    
    if (Test-UbusHealth) {
        Write-Host "✅ ubus fixed by manual restart" -ForegroundColor Green
        return $true
    } else {
        Write-Host "❌ Failed to fix ubus issues" -ForegroundColor Red
        return $false
    }
}

function Get-UbusLogs {
    Write-Host "📋 Checking ubus-related logs..." -ForegroundColor Cyan
    
    $logs = ssh -i $SSH_KEY root@$RUTOS_HOST "logread | grep -i ubus | tail -10"
    if ($logs) {
        Write-Host "📄 Recent ubus logs:" -ForegroundColor Yellow
        Write-Host $logs -ForegroundColor Gray
    } else {
        Write-Host "ℹ️ No recent ubus logs found" -ForegroundColor Yellow
    }
}

function Test-autonomyUbus {
    Write-Host "🔍 Testing autonomy ubus integration..." -ForegroundColor Cyan
    
    $autonomyService = ssh -i $SSH_KEY root@$RUTOS_HOST "ubus list | grep autonomy"
    if ($autonomyService) {
        Write-Host "✅ autonomy ubus service found: $autonomyService" -ForegroundColor Green
        
        # Test autonomy ubus API
        $status = ssh -i $SSH_KEY root@$RUTOS_HOST "ubus call autonomy status 2>&1"
        if ($LASTEXITCODE -eq 0) {
            Write-Host "✅ autonomy ubus API responding" -ForegroundColor Green
            return $true
        } else {
            Write-Host "⚠️ autonomy ubus service found but API not responding" -ForegroundColor Yellow
            return $false
        }
    } else {
        Write-Host "❌ autonomy ubus service not found" -ForegroundColor Red
        return $false
    }
}

function Restart-autonomyWithUbus {
    Write-Host "🔄 Restarting autonomy daemon to register with ubus..." -ForegroundColor Yellow
    
    # Kill existing autonomy processes
    ssh -i $SSH_KEY root@$RUTOS_HOST "pkill autonomyd 2>/dev/null; sleep 2"
    
    # Start autonomy daemon
    ssh -i $SSH_KEY root@$RUTOS_HOST "/tmp/autonomyd-rutx50-fresh -config /etc/config/autonomy -foreground -log-level debug &"
    
    # Wait for startup
    Start-Sleep -Seconds 15
    
    # Test ubus integration
    if (Test-autonomyUbus) {
        Write-Host "✅ autonomy successfully registered with ubus" -ForegroundColor Green
        return $true
    } else {
        Write-Host "❌ autonomy failed to register with ubus" -ForegroundColor Red
        return $false
    }
}

function Run-FullMaintenance {
    Write-Host "🚀 Running full system maintenance..." -ForegroundColor Cyan
    
    # 1. Check ubus health
    Write-Host "`n📋 Step 1: ubus Health Check" -ForegroundColor White
    $ubusHealthy = Test-UbusHealth
    
    if (-not $ubusHealthy) {
        Write-Host "`n📋 Step 2: Fixing ubus Issues" -ForegroundColor White
        $ubusFixed = Fix-UbusIssues
        if ($ubusFixed) {
            $ubusHealthy = $true
        }
    }
    
    # 2. Check ubus logs
    Write-Host "`n📋 Step 3: ubus Log Analysis" -ForegroundColor White
    Get-UbusLogs
    
    # 3. Test autonomy ubus integration
    Write-Host "`n📋 Step 4: autonomy ubus Integration" -ForegroundColor White
    $autonomyUbus = Test-autonomyUbus
    
    if (-not $autonomyUbus -and $ubusHealthy) {
        Write-Host "`n📋 Step 5: Restarting autonomy for ubus registration" -ForegroundColor White
        Restart-autonomyWithUbus
    }
    
    # 4. Final status report
    Write-Host "`n📊 Maintenance Summary:" -ForegroundColor Cyan
    Write-Host "  ubus Health: $(if ($ubusHealthy) { '✅ Healthy' } else { '❌ Unhealthy' })" -ForegroundColor $(if ($ubusHealthy) { 'Green' } else { 'Red' })
    Write-Host "  autonomy ubus: $(if ($autonomyUbus) { '✅ Integrated' } else { '❌ Not Integrated' })" -ForegroundColor $(if ($autonomyUbus) { 'Green' } else { 'Red' })
    
    return @{
        UbusHealthy = $ubusHealthy
        autonomyUbus = $autonomyUbus
    }
}

# Main execution
if ($CheckUbus) {
    Test-UbusHealth
    Get-UbusLogs
    exit 0
}

if ($FixUbus) {
    if (-not (Test-UbusHealth)) {
        Fix-UbusIssues
    } else {
        Write-Host "✅ ubus is already healthy" -ForegroundColor Green
    }
    exit 0
}

if ($FullMaintenance) {
    Run-FullMaintenance
    exit 0
}

if ($TestOnly) {
    Write-Host "🧪 Test Mode - Checking ubus and autonomy integration..." -ForegroundColor Cyan
    Test-UbusHealth
    Test-autonomyUbus
    exit 0
}

# Show usage if no parameters
Write-Host "Enhanced System Maintenance Script" -ForegroundColor Cyan
Write-Host "Usage:" -ForegroundColor White
Write-Host "  .\enhanced-maintenance.ps1 -CheckUbus" -ForegroundColor Gray
Write-Host "  .\enhanced-maintenance.ps1 -FixUbus" -ForegroundColor Gray
Write-Host "  .\enhanced-maintenance.ps1 -FullMaintenance" -ForegroundColor Gray
Write-Host "  .\enhanced-maintenance.ps1 -TestOnly" -ForegroundColor Gray
