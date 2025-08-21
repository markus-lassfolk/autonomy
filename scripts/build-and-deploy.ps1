# RUTOS Build and Deploy Script
# This script builds the autonomy binary and deploys it to RUTOS

param(
    [switch]$Build,
    [switch]$Deploy,
    [switch]$BuildAndDeploy,
    [switch]$Clean,
    [string]$BinaryName = "autonomyd-rutx50-fresh"
)

$SSH_KEY = $env:SSH_KEY_PATH ?? "C:\path\to\your\ssh\key"
$RUTOS_HOST = $env:RUTOS_HOST ?? "192.168.1.1"
$RUTOS_PATH = "/tmp/$BinaryName"

function Build-Binary {
    Write-Host "🔨 Building binary for ARMv7l..." -ForegroundColor Cyan
    
    # Set Go environment variables for ARMv7l cross-compilation
    $env:GOOS = "linux"
    $env:GOARCH = "arm"
    $env:GOARM = "7"
    
    # Build the binary
    $buildCmd = "go build -o $BinaryName ./cmd/autonomyd"
    Write-Host "Executing: $buildCmd" -ForegroundColor Yellow
    Invoke-Expression $buildCmd
    
    if ($LASTEXITCODE -eq 0) {
        $fileInfo = Get-ChildItem $BinaryName | Select-Object Name, Length, LastWriteTime
        Write-Host "✅ Build successful!" -ForegroundColor Green
        Write-Host "Binary: $($fileInfo.Name)" -ForegroundColor White
        Write-Host "Size: $([math]::Round($fileInfo.Length / 1MB, 2)) MB" -ForegroundColor White
        Write-Host "Built: $($fileInfo.LastWriteTime)" -ForegroundColor White
        return $true
    } else {
        Write-Host "❌ Build failed!" -ForegroundColor Red
        return $false
    }
}

function Deploy-Binary {
    param([string]$LocalBinary = $BinaryName)
    
    if (-not (Test-Path $LocalBinary)) {
        Write-Host "❌ Binary not found: $LocalBinary" -ForegroundColor Red
        return $false
    }
    
    Write-Host "📤 Deploying binary to RUTOS..." -ForegroundColor Cyan
    $scpCmd = "scp -i `"$SSH_KEY`" $LocalBinary root@${RUTOS_HOST}:$RUTOS_PATH"
    Write-Host "Executing: $scpCmd" -ForegroundColor Yellow
    Invoke-Expression $scpCmd
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✅ Deploy successful!" -ForegroundColor Green
        
        # Make it executable
        $chmodCmd = "ssh -i `"$SSH_KEY`" root@$RUTOS_HOST `"chmod +x $RUTOS_PATH`""
        Write-Host "Making binary executable..." -ForegroundColor Yellow
        Invoke-Expression $chmodCmd
        
        if ($LASTEXITCODE -eq 0) {
            Write-Host "✅ Binary is now executable on RUTOS" -ForegroundColor Green
            return $true
        } else {
            Write-Host "⚠️ Deploy successful but chmod failed" -ForegroundColor Yellow
            return $true
        }
    } else {
        Write-Host "❌ Deploy failed!" -ForegroundColor Red
        return $false
    }
}

function Test-Binary {
    Write-Host "🧪 Testing binary on RUTOS..." -ForegroundColor Cyan
    $testCmd = "ssh -i `"$SSH_KEY`" root@$RUTOS_HOST `"$RUTOS_PATH --help`""
    Write-Host "Executing: $testCmd" -ForegroundColor Yellow
    Invoke-Expression $testCmd
    
    if ($LASTEXITCODE -eq 0) {
        Write-Host "✅ Binary test successful!" -ForegroundColor Green
        return $true
    } else {
        Write-Host "❌ Binary test failed!" -ForegroundColor Red
        return $false
    }
}

function Clear-Build {
    Write-Host "🧹 Cleaning build artifacts..." -ForegroundColor Cyan
    if (Test-Path $BinaryName) {
        Remove-Item $BinaryName -Force
        Write-Host "✅ Cleaned $BinaryName" -ForegroundColor Green
    }
    
    # Clean any other build artifacts
    Get-ChildItem -Name "autonomyd-*" -ErrorAction SilentlyContinue | ForEach-Object {
        Remove-Item $_ -Force
        Write-Host "✅ Cleaned $_" -ForegroundColor Green
    }
}

# Main execution
if ($Clean) {
    Clear-Build
    exit 0
}

if ($Build) {
    if (Build-Binary) {
        exit 0
    } else {
        exit 1
    }
}

if ($Deploy) {
    if (Deploy-Binary) {
        if (Test-Binary) {
            exit 0
        } else {
            exit 1
        }
    } else {
        exit 1
    }
}

if ($BuildAndDeploy) {
    Write-Host "🚀 Building and deploying..." -ForegroundColor Cyan
    if (Build-Binary) {
        if (Deploy-Binary) {
            if (Test-Binary) {
                Write-Host "🎉 Build and deploy completed successfully!" -ForegroundColor Green
                exit 0
            } else {
                Write-Host "⚠️ Build and deploy successful, but test failed" -ForegroundColor Yellow
                exit 1
            }
        } else {
            Write-Host "❌ Deploy failed after successful build" -ForegroundColor Red
            exit 1
        }
    } else {
        Write-Host "❌ Build failed" -ForegroundColor Red
        exit 1
    }
}

# Show usage if no parameters
Write-Host "RUTOS Build and Deploy Script" -ForegroundColor Cyan
Write-Host "Usage:" -ForegroundColor White
Write-Host "  .\build-and-deploy.ps1 -Build" -ForegroundColor Gray
Write-Host "  .\build-and-deploy.ps1 -Deploy" -ForegroundColor Gray
Write-Host "  .\build-and-deploy.ps1 -BuildAndDeploy" -ForegroundColor Gray
Write-Host "  .\build-and-deploy.ps1 -Clean" -ForegroundColor Gray
Write-Host "  .\build-and-deploy.ps1 -BinaryName 'custom-name'" -ForegroundColor Gray
