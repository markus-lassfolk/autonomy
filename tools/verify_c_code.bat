@echo off
REM Batch file wrapper for C Code Verification Script
REM ================================================
REM This script provides a simple Windows batch interface to the Python C code verifier

setlocal enabledelayedexpansion

REM Default values
set "DIRECTORY=src\c"
set "VERBOSE="
set "STRICT="
set "OUTPUT="
set "EXCLUDE="
set "INCLUDE="

REM Parse command line arguments
:parse_args
if "%~1"=="" goto :run_script
if "%~1"=="-d" (
    set "DIRECTORY=%~2"
    shift
    shift
    goto :parse_args
)
if "%~1"=="--directory" (
    set "DIRECTORY=%~2"
    shift
    shift
    goto :parse_args
)
if "%~1"=="-v" (
    set "VERBOSE=-v"
    shift
    goto :parse_args
)
if "%~1"=="--verbose" (
    set "VERBOSE=-v"
    shift
    goto :parse_args
)
if "%~1"=="-s" (
    set "STRICT=-s"
    shift
    goto :parse_args
)
if "%~1"=="--strict" (
    set "STRICT=-s"
    shift
    goto :parse_args
)
if "%~1"=="-o" (
    set "OUTPUT=-o %~2"
    shift
    shift
    goto :parse_args
)
if "%~1"=="--output" (
    set "OUTPUT=-o %~2"
    shift
    shift
    goto :parse_args
)
if "%~1"=="-h" (
    goto :show_help
)
if "%~1"=="--help" (
    goto :show_help
)
if "%~1"=="-?" (
    goto :show_help
)

REM If we get here, treat the argument as a directory
set "DIRECTORY=%~1"
shift
goto :parse_args

:show_help
echo C Code Verification Tool - Batch Wrapper
echo ========================================
echo.
echo Usage: verify_c_code.bat [OPTIONS] [DIRECTORY]
echo.
echo Options:
echo   -d, --directory DIR  Directory to verify (default: src\c)
echo   -v, --verbose        Verbose output
echo   -s, --strict         Enable strict checking
echo   -o, --output FILE    Output report to file
echo   -h, --help, -?       Show this help
echo.
echo Examples:
echo   verify_c_code.bat
echo   verify_c_code.bat -d src\c -v
echo   verify_c_code.bat -s -o report.txt
echo.
echo Requirements:
echo   - Python 3.6 or higher
echo   - GCC or Clang compiler (for syntax checking)
echo.
goto :eof

:run_script
REM Get the script directory
set "SCRIPT_DIR=%~dp0"
set "PYTHON_SCRIPT=%SCRIPT_DIR%verify_c_code.py"

REM Check if Python script exists
if not exist "%PYTHON_SCRIPT%" (
    echo Error: Python verification script not found at: %PYTHON_SCRIPT%
    echo Please ensure verify_c_code.py is in the same directory as this batch file.
    exit /b 1
)

REM Check if Python is available
python --version >nul 2>&1
if errorlevel 1 (
    echo Error: Python is not installed or not in PATH
    echo Please install Python 3.6 or higher and ensure it's in your PATH.
    echo Download from: https://www.python.org/downloads/
    exit /b 1
)

REM Convert Windows paths to Unix-style for Python script
set "UNIX_DIRECTORY=%DIRECTORY:\=/%"

REM Build command
set "CMD=python "%PYTHON_SCRIPT%" "%UNIX_DIRECTORY%" %VERBOSE% %STRICT% %OUTPUT%"

REM Display what we're about to run
echo Starting C Code Verification...
echo Directory: %DIRECTORY%
if defined VERBOSE echo Verbose: Enabled
if defined STRICT echo Strict: Enabled
if defined OUTPUT echo Output: %OUTPUT%
echo.

REM Run the Python script
%CMD%

REM Exit with the same code as the Python script
exit /b %errorlevel%
