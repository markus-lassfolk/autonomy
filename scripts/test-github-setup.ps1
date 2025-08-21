# Quick test script to verify GitHub setup after configuration changes

Write-Host "🧪 Testing GitHub Configuration After Setup" -ForegroundColor Green
Write-Host "=============================================" -ForegroundColor Green

# Test 1: Verify Actions are enabled
Write-Host "`n1️⃣ Testing GitHub Actions Status..." -ForegroundColor Cyan
try {
    $repoInfo = gh api "repos/$env:GITHUB_REPOSITORY" | ConvertFrom-Json
    if ($repoInfo.has_actions) {
        Write-Host "✅ GitHub Actions are enabled" -ForegroundColor Green
    } else {
        Write-Host "❌ GitHub Actions are still disabled" -ForegroundColor Red
        Write-Host "   Please enable them in repository settings" -ForegroundColor Yellow
    }
} catch {
    Write-Host "❌ Cannot check Actions status" -ForegroundColor Red
}

# Test 2: Verify GITHUB_TOKEN secret
Write-Host "`n2️⃣ Testing GITHUB_TOKEN Secret..." -ForegroundColor Cyan
try {
    $secretInfo = gh api "repos/$env:GITHUB_REPOSITORY/actions/secrets/GITHUB_TOKEN" 2>$null
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✅ GITHUB_TOKEN secret is configured" -ForegroundColor Green
    } else {
        Write-Host "❌ GITHUB_TOKEN secret is missing" -ForegroundColor Red
    }
} catch {
    Write-Host "❌ Cannot verify GITHUB_TOKEN secret" -ForegroundColor Red
}

# Test 3: Trigger a simple workflow to test
Write-Host "`n3️⃣ Testing Workflow Execution..." -ForegroundColor Cyan
try {
    Write-Host "Triggering test workflow..." -ForegroundColor Blue
    gh workflow run "build-packages.yml" 2>$null
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✅ Workflow triggered successfully" -ForegroundColor Green
        Write-Host "   Check: https://github.com/markus-lassfolk/autonomy/actions" -ForegroundColor Blue
    } else {
        Write-Host "❌ Cannot trigger workflow" -ForegroundColor Red
    }
} catch {
    Write-Host "❌ Workflow trigger failed" -ForegroundColor Red
}

# Test 4: Check webhook endpoint
Write-Host "`n4️⃣ Testing Webhook Configuration..." -ForegroundColor Cyan
try {
    gh workflow run "webhook-receiver.yml" --field test_payload='{"device_id":"test","severity":"info","scenario":"test","note":"Configuration test"}' 2>$null
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✅ Webhook workflow triggered successfully" -ForegroundColor Green
    } else {
        Write-Host "❌ Cannot trigger webhook workflow" -ForegroundColor Red
    }
} catch {
    Write-Host "❌ Webhook test failed" -ForegroundColor Red
}

Write-Host "`n📊 Test Summary" -ForegroundColor Yellow
Write-Host "===============" -ForegroundColor Yellow
Write-Host "If all tests pass, your autonomous workflows are ready!" -ForegroundColor Green
Write-Host "Monitor workflow runs at: https://github.com/markus-lassfolk/autonomy/actions" -ForegroundColor Blue

Write-Host "`n✅ Configuration test complete!" -ForegroundColor Green
