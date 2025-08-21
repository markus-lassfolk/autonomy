#!/usr/bin/env pwsh

# Fix YAML indentation issues in workflow files
# This script corrects common indentation problems in GitHub Actions workflow files

$workflowFiles = @(
    ".github/workflows/package-release.yml",
    ".github/workflows/release.yml", 
    ".github/workflows/ci.yml",
    ".github/workflows/rutos-test-environment.yml"
)

foreach ($file in $workflowFiles) {
    if (Test-Path $file) {
        Write-Host "Fixing indentation in $file..."
        
        # Read the file content
        $content = Get-Content $file -Raw
        
        # Fix common indentation issues
        $content = $content -replace '^\s{5}-\s+name:', '    - name:'  # Fix 5-space step indentation
        $content = $content -replace '^\s{6}uses:', '      uses:'     # Fix 6-space uses indentation
        $content = $content -replace '^\s{7}with:', '       with:'    # Fix 7-space with indentation
        $content = $content -replace '^\s{8}name:', '         name:'  # Fix 8-space name indentation
        $content = $content -replace '^\s{8}path:', '         path:'  # Fix 8-space path indentation
        $content = $content -replace '^\s{8}retention-days:', '         retention-days:'  # Fix 8-space retention-days indentation
        
        # Fix specific problematic patterns
        $content = $content -replace '^\s{9}build/', '           build/'  # Fix 9-space build/ indentation
        $content = $content -replace '^\s{9}packages/', '           packages/'  # Fix 9-space packages/ indentation
        $content = $content -replace '^\s{9}artifacts/', '           artifacts/'  # Fix 9-space artifacts/ indentation
        
        # Write the fixed content back
        Set-Content $file -Value $content -NoNewline
        
        Write-Host "✅ Fixed $file"
    } else {
        Write-Host "⚠️  File not found: $file"
    }
}

Write-Host "🎉 YAML indentation fixes completed!"
