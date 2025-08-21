# GitHub Configuration Verification Script
# This script verifies that all required secrets and variables are configured correctly

Write-Host "🔐 GitHub Configuration Verification" -ForegroundColor Green
Write-Host "====================================" -ForegroundColor Green

# Required Secrets
$requiredSecrets = @{
    "WEBHOOK_SECRET" = "HMAC secret key for webhook validation (32+ characters recommended)"
    "AUTONOMY_GH_TOKEN" = "Personal Access Token with repo, workflow, and packages permissions"
    "COPILOT_TOKEN" = "GitHub Copilot API token or PAT with Copilot access"
    "DOCKERHUB_USERNAME" = "Docker Hub username for publishing images"
    "DOCKERHUB_TOKEN" = "Docker Hub access token for publishing"
}

# Required Variables
$requiredVariables = @{
    "SUPPORTED_VERSIONS" = "Comma-separated list of supported RUTOS versions (e.g., 'RUTX_R_00.07.17,RUTX_R_00.07.18')"
    "MIN_SEVERITY" = "Minimum severity level for alerts (debug, info, warn, error, critical)"
    "COPILOT_ENABLED" = "Enable Copilot autonomous features (true/false)"
    "AUTO_ASSIGN" = "Auto-assign issues to Copilot (true/false)"
    "BUILD_PLATFORMS" = "Platforms to build for (optional, defaults to all)"
    "DOCKER_REGISTRY" = "Docker registry URL (optional, defaults to Docker Hub)"
}

# Optional Secrets (for enhanced features)
$optionalSecrets = @{
    "SLACK_WEBHOOK_URL" = "Slack webhook for notifications"
    "DISCORD_WEBHOOK_URL" = "Discord webhook for notifications"
    "TELEGRAM_BOT_TOKEN" = "Telegram bot token for notifications"
    "TELEGRAM_CHAT_ID" = "Telegram chat ID for notifications"
}

function Test-GitHubCLI {
    Write-Host "`n🔍 Testing GitHub CLI..." -ForegroundColor Cyan
    
    try {
        $null = Get-Command gh -ErrorAction Stop
        Write-Host "✅ GitHub CLI is installed" -ForegroundColor Green
        
        # Test authentication
        $authStatus = gh auth status 2>&1
        if ($LASTEXITCODE -eq 0) {
            Write-Host "✅ GitHub CLI is authenticated" -ForegroundColor Green
            return $true
        } else {
            Write-Host "❌ GitHub CLI is not authenticated" -ForegroundColor Red
            Write-Host "   Run: gh auth login" -ForegroundColor Yellow
            return $false
        }
    } catch {
        Write-Host "❌ GitHub CLI is not installed" -ForegroundColor Red
        Write-Host "   Install from: https://cli.github.com/" -ForegroundColor Yellow
        return $false
    }
}

function Test-Secrets {
    param([hashtable]$secrets, [string]$type = "Required")
    
    Write-Host "`n🔐 Testing $type Secrets..." -ForegroundColor Cyan
    
    $results = @{}
    $allPresent = $true
    
    foreach ($secret in $secrets.Keys) {
        $description = $secrets[$secret]
        
        try {
            # Try to get secret (this will fail if not authenticated or secret doesn't exist)
            $secretInfo = gh api "repos/$env:GITHUB_REPOSITORY/actions/secrets/$secret" 2>$null
            
            if ($LASTEXITCODE -eq 0) {
                Write-Host "✅ $secret : Present" -ForegroundColor Green
                $results[$secret] = "Present"
            } else {
                Write-Host "❌ $secret : Missing" -ForegroundColor Red
                Write-Host "   Description: $description" -ForegroundColor Yellow
                $results[$secret] = "Missing"
                if ($type -eq "Required") { $allPresent = $false }
            }
        } catch {
            Write-Host "❌ $secret : Cannot verify (check authentication)" -ForegroundColor Red
            $results[$secret] = "Cannot verify"
            if ($type -eq "Required") { $allPresent = $false }
        }
    }
    
    return @{
        "Results" = $results
        "AllPresent" = $allPresent
    }
}

function Test-Variables {
    param([hashtable]$variables)
    
    Write-Host "`n📋 Testing Repository Variables..." -ForegroundColor Cyan
    
    $results = @{}
    $allPresent = $true
    
    foreach ($variable in $variables.Keys) {
        $description = $variables[$variable]
        
        try {
            # Try to get variable
            $variableInfo = gh api "repos/$env:GITHUB_REPOSITORY/actions/variables/$variable" 2>$null
            
            if ($LASTEXITCODE -eq 0) {
                $value = ($variableInfo | ConvertFrom-Json).value
                Write-Host "✅ $variable : $value" -ForegroundColor Green
                $results[$variable] = $value
            } else {
                Write-Host "❌ $variable : Missing" -ForegroundColor Red
                Write-Host "   Description: $description" -ForegroundColor Yellow
                $results[$variable] = "Missing"
                $allPresent = $false
            }
        } catch {
            Write-Host "❌ $variable : Cannot verify (check authentication)" -ForegroundColor Red
            $results[$variable] = "Cannot verify"
            $allPresent = $false
        }
    }
    
    return @{
        "Results" = $results
        "AllPresent" = $allPresent
    }
}

function Test-WorkflowPermissions {
    Write-Host "`n🔒 Testing Workflow Permissions..." -ForegroundColor Cyan
    
    try {
        # Get repository info
        $repoInfo = gh api "repos/$env:GITHUB_REPOSITORY" | ConvertFrom-Json
        
        # Check if Actions are enabled
        if ($repoInfo.has_actions) {
            Write-Host "✅ GitHub Actions are enabled" -ForegroundColor Green
        } else {
            Write-Host "❌ GitHub Actions are disabled" -ForegroundColor Red
            return $false
        }
        
        # Check workflow permissions
        $actionsPermissions = gh api "repos/$env:GITHUB_REPOSITORY/actions/permissions" | ConvertFrom-Json
        
        if ($actionsPermissions.enabled) {
            Write-Host "✅ Workflow permissions are enabled" -ForegroundColor Green
            Write-Host "   Allowed actions: $($actionsPermissions.allowed_actions)" -ForegroundColor Blue
        } else {
            Write-Host "❌ Workflow permissions are disabled" -ForegroundColor Red
            return $false
        }
        
        return $true
    } catch {
        Write-Host "❌ Cannot verify workflow permissions" -ForegroundColor Red
        return $false
    }
}

function Test-WorkflowRuns {
    Write-Host "`n🏃 Testing Recent Workflow Runs..." -ForegroundColor Cyan
    
    try {
        $workflows = gh api "repos/$env:GITHUB_REPOSITORY/actions/workflows" | ConvertFrom-Json
        
        $recentFailures = @()
        $totalWorkflows = 0
        $successfulWorkflows = 0
        
        foreach ($workflow in $workflows.workflows) {
            $totalWorkflows++
            
            # Get recent runs for this workflow
            $runs = gh api "repos/$env:GITHUB_REPOSITORY/actions/workflows/$($workflow.id)/runs?per_page=5" | ConvertFrom-Json
            
            if ($runs.workflow_runs.Count -gt 0) {
                $latestRun = $runs.workflow_runs[0]
                
                if ($latestRun.conclusion -eq "success") {
                    Write-Host "✅ $($workflow.name) : Last run successful" -ForegroundColor Green
                    $successfulWorkflows++
                } elseif ($latestRun.conclusion -eq "failure") {
                    Write-Host "❌ $($workflow.name) : Last run failed" -ForegroundColor Red
                    $recentFailures += $workflow.name
                } elseif ($latestRun.status -eq "in_progress") {
                    Write-Host "🔄 $($workflow.name) : Currently running" -ForegroundColor Yellow
                } else {
                    Write-Host "⚠️  $($workflow.name) : Status: $($latestRun.conclusion)" -ForegroundColor Yellow
                }
            } else {
                Write-Host "ℹ️  $($workflow.name) : No runs yet" -ForegroundColor Blue
            }
        }
        
        Write-Host "`nWorkflow Summary:" -ForegroundColor Blue
        Write-Host "  Total workflows: $totalWorkflows" -ForegroundColor White
        Write-Host "  Successful: $successfulWorkflows" -ForegroundColor Green
        Write-Host "  Failed: $($recentFailures.Count)" -ForegroundColor Red
        
        if ($recentFailures.Count -gt 0) {
            Write-Host "  Recent failures: $($recentFailures -join ', ')" -ForegroundColor Red
        }
        
        return $recentFailures.Count -eq 0
    } catch {
        Write-Host "❌ Cannot verify workflow runs" -ForegroundColor Red
        return $false
    }
}

function Show-SetupInstructions {
    param([hashtable]$missingSecrets, [hashtable]$missingVariables)
    
    Write-Host "`n📋 Setup Instructions" -ForegroundColor Yellow
    Write-Host "=====================" -ForegroundColor Yellow
    
    if ($missingSecrets.Count -gt 0) {
        Write-Host "`n🔐 Missing Secrets - Set these in GitHub:" -ForegroundColor Red
        Write-Host "Repository Settings → Secrets and variables → Actions → New repository secret" -ForegroundColor Blue
        
        foreach ($secret in $missingSecrets.Keys) {
            Write-Host "`n$secret :" -ForegroundColor White
            Write-Host "  Description: $($requiredSecrets[$secret])" -ForegroundColor Gray
            
            switch ($secret) {
                "WEBHOOK_SECRET" {
                    Write-Host "  Example: openssl rand -hex 32" -ForegroundColor Green
                }
                "GITHUB_TOKEN" {
                    Write-Host "  Create at: https://github.com/settings/tokens" -ForegroundColor Green
                    Write-Host "  Permissions: repo, workflow, packages, actions" -ForegroundColor Green
                }
                "COPILOT_TOKEN" {
                    Write-Host "  Use same as GITHUB_TOKEN or create Copilot-specific token" -ForegroundColor Green
                }
                "DOCKERHUB_USERNAME" {
                    Write-Host "  Your Docker Hub username" -ForegroundColor Green
                }
                "DOCKERHUB_TOKEN" {
                    Write-Host "  Create at: https://hub.docker.com/settings/security" -ForegroundColor Green
                }
            }
        }
    }
    
    if ($missingVariables.Count -gt 0) {
        Write-Host "`n📋 Missing Variables - Set these in GitHub:" -ForegroundColor Red
        Write-Host "Repository Settings → Secrets and variables → Actions → Variables tab → New repository variable" -ForegroundColor Blue
        
        foreach ($variable in $missingVariables.Keys) {
            Write-Host "`n$variable :" -ForegroundColor White
            Write-Host "  Description: $($requiredVariables[$variable])" -ForegroundColor Gray
            
            switch ($variable) {
                "SUPPORTED_VERSIONS" {
                    Write-Host "  Example: RUTX_R_00.07.17,RUTX_R_00.07.18,RUTX_R_00.08.00" -ForegroundColor Green
                }
                "MIN_SEVERITY" {
                    Write-Host "  Example: warn" -ForegroundColor Green
                }
                "COPILOT_ENABLED" {
                    Write-Host "  Example: true" -ForegroundColor Green
                }
                "AUTO_ASSIGN" {
                    Write-Host "  Example: true" -ForegroundColor Green
                }
            }
        }
    }
}

function Test-WebhookEndpoint {
    Write-Host "`n🌐 Testing Webhook Configuration..." -ForegroundColor Cyan
    
    # Check if webhook server script exists
    if (Test-Path "scripts/webhook-server.go") {
        Write-Host "✅ Webhook server script exists" -ForegroundColor Green
        
        # Check if it can be compiled
        try {
            Push-Location "scripts"
            go build -o webhook-server webhook-server.go 2>$null
            if ($LASTEXITCODE -eq 0) {
                Write-Host "✅ Webhook server compiles successfully" -ForegroundColor Green
                Remove-Item "webhook-server*" -ErrorAction SilentlyContinue
            } else {
                Write-Host "❌ Webhook server compilation failed" -ForegroundColor Red
            }
            Pop-Location
        } catch {
            Write-Host "❌ Cannot test webhook server compilation" -ForegroundColor Red
            Pop-Location
        }
    } else {
        Write-Host "❌ Webhook server script missing" -ForegroundColor Red
    }
    
    # Check webhook receiver workflow
    if (Test-Path ".github/workflows/webhook-receiver.yml") {
        Write-Host "✅ Webhook receiver workflow exists" -ForegroundColor Green
    } else {
        Write-Host "❌ Webhook receiver workflow missing" -ForegroundColor Red
    }
}

# Main execution
Write-Host "Starting GitHub configuration verification..." -ForegroundColor Blue

# Set repository if not already set
if (-not $env:GITHUB_REPOSITORY) {
    try {
        $remoteUrl = git config --get remote.origin.url
        if ($remoteUrl -match "github\.com[:/]([^/]+)/([^/.]+)") {
            $env:GITHUB_REPOSITORY = "$($matches[1])/$($matches[2])"
            Write-Host "📍 Repository: $env:GITHUB_REPOSITORY" -ForegroundColor Blue
        } else {
            Write-Host "❌ Cannot determine GitHub repository" -ForegroundColor Red
            exit 1
        }
    } catch {
        Write-Host "❌ Cannot determine GitHub repository" -ForegroundColor Red
        exit 1
    }
}

# Test GitHub CLI
$cliWorking = Test-GitHubCLI

if (-not $cliWorking) {
    Write-Host "`n❌ GitHub CLI is required for verification" -ForegroundColor Red
    Write-Host "Please install and authenticate GitHub CLI first" -ForegroundColor Yellow
    exit 1
}

# Test secrets
$secretResults = Test-Secrets $requiredSecrets "Required"
$optionalSecretResults = Test-Secrets $optionalSecrets "Optional"

# Test variables
$variableResults = Test-Variables $requiredVariables

# Test permissions
$permissionsOk = Test-WorkflowPermissions

# Test workflow runs
$workflowsOk = Test-WorkflowRuns

# Test webhook configuration
Test-WebhookEndpoint

# Summary
Write-Host "`n📊 Configuration Summary" -ForegroundColor Yellow
Write-Host "========================" -ForegroundColor Yellow

$totalIssues = 0

# Count missing required items
$missingSecrets = @{}
$missingVariables = @{}

foreach ($secret in $secretResults.Results.Keys) {
    if ($secretResults.Results[$secret] -eq "Missing") {
        $missingSecrets[$secret] = $requiredSecrets[$secret]
        $totalIssues++
    }
}

foreach ($variable in $variableResults.Results.Keys) {
    if ($variableResults.Results[$variable] -eq "Missing") {
        $missingVariables[$variable] = $requiredVariables[$variable]
        $totalIssues++
    }
}

if (-not $permissionsOk) { $totalIssues++ }
if (-not $workflowsOk) { $totalIssues++ }

Write-Host "Required Secrets: $(($secretResults.Results.Keys | Where-Object { $secretResults.Results[$_] -eq 'Present' }).Count)/$($requiredSecrets.Count) configured" -ForegroundColor $(if ($secretResults.AllPresent) { "Green" } else { "Red" })
Write-Host "Repository Variables: $(($variableResults.Results.Keys | Where-Object { $variableResults.Results[$_] -ne 'Missing' -and $variableResults.Results[$_] -ne 'Cannot verify' }).Count)/$($requiredVariables.Count) configured" -ForegroundColor $(if ($variableResults.AllPresent) { "Green" } else { "Red" })
Write-Host "Workflow Permissions: $(if ($permissionsOk) { 'OK' } else { 'Issues' })" -ForegroundColor $(if ($permissionsOk) { "Green" } else { "Red" })
Write-Host "Recent Workflow Runs: $(if ($workflowsOk) { 'OK' } else { 'Failures' })" -ForegroundColor $(if ($workflowsOk) { "Green" } else { "Red" })

if ($totalIssues -eq 0) {
    Write-Host "`n🎉 All configurations are correct!" -ForegroundColor Green
    Write-Host "Your autonomous workflows are ready to use!" -ForegroundColor Green
} else {
    Write-Host "`n⚠️  $totalIssues configuration issues found" -ForegroundColor Yellow
    Show-SetupInstructions $missingSecrets $missingVariables
}

Write-Host "`n✅ Verification complete!" -ForegroundColor Green
