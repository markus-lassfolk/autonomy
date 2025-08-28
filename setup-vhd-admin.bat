@echo off
echo =========================================
echo Launching VHD Setup as Administrator
echo =========================================
echo.
echo This will create a 50GB VHD at D:\WSL\rutos-sdk.vhdx
echo.
pause

:: Run PowerShell as Administrator
powershell -Command "Start-Process PowerShell -ArgumentList '-ExecutionPolicy Bypass -File ""%~dp0setup-wsl-vhd.ps1"" -VhdPath ""D:\WSL\rutos-sdk.vhdx"" -SizeGB 50' -Verb RunAs"

echo.
echo Script launched in Administrator PowerShell window.
echo Please check that window for progress.
pause


