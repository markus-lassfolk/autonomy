#!/usr/bin/env python3
"""
Test script for the enhanced verification system
===============================================

This script demonstrates how to use the enhanced verification script
to check for compilation issues in the autonomy-daemon.
"""

import subprocess
import sys
from pathlib import Path

def run_verification(directory="src/c/autonomy-daemon", verbose=True, strict=True):
    """Run the enhanced verification script"""
    print("🔍 Running Enhanced C Code Verification")
    print("=" * 50)
    
    # Run the main verification script
    cmd = [
        sys.executable, 
        "tools/verify_c_code.py",
        directory,
        "-v" if verbose else "",
        "-s" if strict else "",
        "-o", "verification_report.txt"
    ]
    
    # Remove empty strings
    cmd = [arg for arg in cmd if arg]
    
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
        
        print("STDOUT:")
        print(result.stdout)
        
        if result.stderr:
            print("STDERR:")
            print(result.stderr)
        
        print(f"Return code: {result.returncode}")
        
        # Check if report was generated
        if Path("verification_report.txt").exists():
            print("\n📄 Report generated: verification_report.txt")
            with open("verification_report.txt", 'r') as f:
                report_content = f.read()
                print("Report preview (first 500 chars):")
                print(report_content[:500] + "..." if len(report_content) > 500 else report_content)
        
        return result.returncode == 0
        
    except subprocess.TimeoutExpired:
        print("❌ Verification timed out after 5 minutes")
        return False
    except Exception as e:
        print(f"❌ Error running verification: {e}")
        return False

def run_compilation_checker(directory="src/c/autonomy-daemon", verbose=True):
    """Run the specialized compilation checker"""
    print("\n🔧 Running Specialized Compilation Checker")
    print("=" * 50)
    
    cmd = [
        sys.executable,
        "tools/check_compilation_issues.py", 
        directory,
        "-v" if verbose else "",
        "-o", "compilation_issues_report.txt"
    ]
    
    # Remove empty strings
    cmd = [arg for arg in cmd if arg]
    
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
        
        print("STDOUT:")
        print(result.stdout)
        
        if result.stderr:
            print("STDERR:")
            print(result.stderr)
        
        print(f"Return code: {result.returncode}")
        
        # Check if report was generated
        if Path("compilation_issues_report.txt").exists():
            print("\n📄 Compilation issues report generated: compilation_issues_report.txt")
            with open("compilation_issues_report.txt", 'r') as f:
                report_content = f.read()
                print("Report preview (first 500 chars):")
                print(report_content[:500] + "..." if len(report_content) > 500 else report_content)
        
        return result.returncode == 0
        
    except subprocess.TimeoutExpired:
        print("❌ Compilation checker timed out after 2 minutes")
        return False
    except Exception as e:
        print(f"❌ Error running compilation checker: {e}")
        return False

def main():
    """Main test function"""
    print("🧪 Testing Enhanced Verification System")
    print("=" * 60)
    
    # Test the main verification script
    print("\n1. Testing main verification script...")
    main_success = run_verification()
    
    # Test the specialized compilation checker
    print("\n2. Testing specialized compilation checker...")
    compilation_success = run_compilation_checker()
    
    # Summary
    print("\n📊 Test Summary")
    print("=" * 30)
    print(f"Main verification: {'✅ PASSED' if main_success else '❌ FAILED'}")
    print(f"Compilation checker: {'✅ PASSED' if compilation_success else '❌ FAILED'}")
    
    if main_success and compilation_success:
        print("\n🎉 All tests passed! The verification system is working correctly.")
        return 0
    else:
        print("\n⚠️ Some tests failed. Check the output above for details.")
        return 1

if __name__ == "__main__":
    sys.exit(main())
