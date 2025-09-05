#!/usr/bin/env python3
"""
Compilation Issues Checker
==========================

This script specifically checks for the compilation issues that were fixed in the autonomy-daemon:
- Missing function implementations (declared but not defined)
- Missing struct members
- Missing type definitions
- Incorrect libubox function usage
- Format string issues
- Include path problems
- Global variable declarations

Usage:
    python3 check_compilation_issues.py [directory]
"""

import os
import sys
import re
import subprocess
from pathlib import Path
from typing import List, Dict, Set, Tuple, Optional
from dataclasses import dataclass
from collections import defaultdict

@dataclass
class CompilationIssue:
    """Container for compilation issues"""
    file_path: str
    line_number: int = 0
    severity: str = "error"  # error, warning
    category: str = ""
    message: str = ""
    suggestion: str = ""
    code_snippet: str = ""

class CompilationChecker:
    """Main compilation issue checker"""
    
    def __init__(self, verbose: bool = False):
        self.verbose = verbose
        self.issues: List[CompilationIssue] = []
        self.files: Dict[str, str] = {}  # {file_path: content}
        self.functions: Dict[str, List[Dict]] = defaultdict(list)  # {func_name: [locations]}
        self.static_functions: Dict[str, List[Dict]] = defaultdict(list)
        self.types: Set[str] = set()
        self.structs: Dict[str, Dict] = {}  # {struct_name: {file, line, members}}
        self.globals: Set[str] = set()
    
    def log(self, message: str):
        """Log message if verbose"""
        if self.verbose:
            print(f"🔍 {message}")
    
    def add_issue(self, file_path: str, message: str, severity: str = "error", 
                  line_number: int = 0, category: str = "", suggestion: str = "", 
                  code_snippet: str = ""):
        """Add a compilation issue"""
        issue = CompilationIssue(
            file_path=file_path,
            line_number=line_number,
            severity=severity,
            category=category,
            message=message,
            suggestion=suggestion,
            code_snippet=code_snippet
        )
        self.issues.append(issue)
    
    def find_c_files(self, directory: str) -> List[str]:
        """Find all C source files in directory"""
        c_files = []
        directory = Path(directory)
        
        if not directory.exists():
            self.add_issue("", f"Directory {directory} does not exist", "error")
            return c_files
        
        for pattern in ["*.c", "*.h"]:
            for file_path in directory.rglob(pattern):
                if file_path.is_file():
                    c_files.append(str(file_path))
        
        self.log(f"Found {len(c_files)} C files")
        return c_files
    
    def load_file(self, file_path: str) -> str:
        """Load file content"""
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
            self.files[file_path] = content
            return content
        except Exception as e:
            self.add_issue(file_path, f"Failed to load file: {e}", "error")
            return ""
    
    def extract_functions(self, file_path: str, content: str):
        """Extract function declarations and definitions"""
        lines = content.split('\n')
        
        for i, line in enumerate(lines):
            # Function definition pattern
            func_def_match = re.search(r'^(\w+(?:\s+\w+)*)\s+(\w+)\s*\([^)]*\)\s*\{', line)
            if func_def_match:
                func_name = func_def_match.group(2)
                self.functions[func_name].append({
                    'file': file_path,
                    'line': i + 1,
                    'type': 'definition',
                    'signature': f"{func_def_match.group(1)} {func_name}"
                })
            
            # Function declaration pattern
            func_decl_match = re.search(r'^(\w+(?:\s+\w+)*)\s+(\w+)\s*\([^)]*\)\s*;', line)
            if func_decl_match:
                func_name = func_decl_match.group(2)
                self.functions[func_name].append({
                    'file': file_path,
                    'line': i + 1,
                    'type': 'declaration',
                    'signature': f"{func_decl_match.group(1)} {func_name}"
                })
            
            # Static function declaration pattern
            static_decl_match = re.search(r'static\s+(\w+(?:\s+\w+)*)\s+(\w+)\s*\([^)]*\)\s*;', line)
            if static_decl_match:
                func_name = static_decl_match.group(2)
                self.static_functions[func_name].append({
                    'file': file_path,
                    'line': i + 1,
                    'type': 'declaration',
                    'signature': f"static {static_decl_match.group(1)} {func_name}"
                })
            
            # Static function definition pattern
            static_def_match = re.search(r'static\s+(\w+(?:\s+\w+)*)\s+(\w+)\s*\([^)]*\)\s*\{', line)
            if static_def_match:
                func_name = static_def_match.group(2)
                self.static_functions[func_name].append({
                    'file': file_path,
                    'line': i + 1,
                    'type': 'definition',
                    'signature': f"static {static_def_match.group(1)} {func_name}"
                })
    
    def extract_types(self, file_path: str, content: str):
        """Extract type definitions"""
        for line in content.split('\n'):
            # Typedef pattern
            typedef_match = re.search(r'typedef\s+.*\s+(\w+)\s*;', line)
            if typedef_match:
                self.types.add(typedef_match.group(1))
    
    def extract_structs(self, file_path: str, content: str):
        """Extract struct definitions and members"""
        lines = content.split('\n')
        
        for i, line in enumerate(lines):
            # Struct definition pattern
            struct_match = re.search(r'typedef\s+struct\s+(\w+)\s*\{([^}]+)\}\s*(\w+)\s*;', line, re.DOTALL)
            if struct_match:
                struct_name = struct_match.group(3)
                struct_content = struct_match.group(2)
                
                # Extract members
                members = []
                for member_line in struct_content.split('\n'):
                    member_match = re.search(r'\s*(\w+(?:\s+\w+)*)\s+(\w+)\s*;', member_line)
                    if member_match:
                        members.append(member_match.group(2))
                
                self.structs[struct_name] = {
                    'file': file_path,
                    'line': i + 1,
                    'members': members
                }
    
    def extract_globals(self, file_path: str, content: str):
        """Extract global variable declarations"""
        for line in content.split('\n'):
            # Global variable pattern
            global_match = re.search(r'^(\w+(?:\s+\w+)*)\s+(\w+)\s*[=;]', line)
            if global_match and global_match.group(2).startswith('g_'):
                self.globals.add(global_match.group(2))
    
    def check_missing_function_implementations(self):
        """Check for functions declared but not implemented"""
        self.log("Checking for missing function implementations...")
        
        for func_name, locations in self.functions.items():
            declarations = [loc for loc in locations if loc['type'] == 'declaration']
            definitions = [loc for loc in locations if loc['type'] == 'definition']
            
            if declarations and not definitions:
                for decl in declarations:
                    self.add_issue(
                        decl['file'],
                        f"Function declared but not implemented: {func_name}",
                        "error",
                        decl['line'],
                        "missing_implementation",
                        f"Implement function: {decl['signature']} or remove declaration"
                    )
    
    def check_missing_static_function_implementations(self):
        """Check for static functions declared but not implemented"""
        self.log("Checking for missing static function implementations...")
        
        for func_name, locations in self.static_functions.items():
            declarations = [loc for loc in locations if loc['type'] == 'declaration']
            definitions = [loc for loc in locations if loc['type'] == 'definition']
            
            if declarations and not definitions:
                for decl in declarations:
                    self.add_issue(
                        decl['file'],
                        f"Static function declared but not implemented: {func_name}",
                        "error",
                        decl['line'],
                        "missing_static_implementation",
                        f"Implement static function {func_name} or remove declaration"
                    )
    
    def check_missing_type_definitions(self):
        """Check for missing type definitions"""
        self.log("Checking for missing type definitions...")
        
        required_types = [
            'starlink_collection_result_t',
            'starlink_lla_position_t', 
            'starlink_health_t',
            'starlink_tracker_t'
        ]
        
        for file_path, content in self.files.items():
            for i, line in enumerate(content.split('\n')):
                for required_type in required_types:
                    if required_type in line and required_type not in self.types:
                        self.add_issue(
                            file_path,
                            f"Missing type definition: {required_type}",
                            "error",
                            i + 1,
                            "missing_type_definition",
                            f"Define type {required_type} in appropriate header file"
                        )
    
    def check_missing_struct_members(self):
        """Check for missing struct members"""
        self.log("Checking for missing struct members...")
        
        required_members = {
            'autonomy_state': [
                'running', 'gps_enabled', 'current_lat', 'current_lon', 
                'current_accuracy', 'current_confidence', 'last_gps_update',
                'location_status', 'movement_detected', 'last_movement_check'
            ],
            'starlink_device_info_t': ['lat', 'lon'],
            'system_health': [
                'starlink_health', 'uci_health', 'overlay_health', 'services_health',
                'network_health', 'database_health', 'time_health', 'logs_health',
                'overall_score', 'last_check', 'status'
            ]
        }
        
        for file_path, content in self.files.items():
            for i, line in enumerate(content.split('\n')):
                for struct_name, required_member_list in required_members.items():
                    if struct_name in line and '.' in line:
                        for member in required_member_list:
                            if f'.{member}' in line:
                                # Check if struct is defined and has the member
                                if struct_name in self.structs:
                                    if member not in self.structs[struct_name]['members']:
                                        self.add_issue(
                                            file_path,
                                            f"Missing struct member: {struct_name}.{member}",
                                            "error",
                                            i + 1,
                                            "missing_struct_member",
                                            f"Add member {member} to struct {struct_name}"
                                        )
    
    def check_libubox_compatibility(self):
        """Check for libubox compatibility issues"""
        self.log("Checking libubox compatibility...")
        
        for file_path, content in self.files.items():
            for i, line in enumerate(content.split('\n')):
                # Check for blobmsg_add_f32 usage
                if 'blobmsg_add_f32' in line:
                    self.add_issue(
                        file_path,
                        f"blobmsg_add_f32 function does not exist in libubox: {line.strip()}",
                        "error",
                        i + 1,
                        "libubox_compatibility",
                        "Use blobmsg_add_double instead of blobmsg_add_f32"
                    )
                
                # Check for const struct blob_attr ** usage
                if 'const struct blob_attr **' in line and 'blobmsg_parse' in line:
                    self.add_issue(
                        file_path,
                        f"Incompatible pointer type in blobmsg_parse: {line.strip()}",
                        "error",
                        i + 1,
                        "libubox_compatibility",
                        "Remove 'const' from struct blob_attr ** parameter"
                    )
    
    def check_format_string_issues(self):
        """Check for format string issues"""
        self.log("Checking format string issues...")
        
        for file_path, content in self.files.items():
            for i, line in enumerate(content.split('\n')):
                # Check for incorrect format specifiers
                if 'sscanf' in line and '%lu' in line and 'uint64_t' in line:
                    self.add_issue(
                        file_path,
                        f"Incorrect format specifier for uint64_t: {line.strip()}",
                        "error",
                        i + 1,
                        "format_string",
                        "Use %llu for uint64_t instead of %lu"
                    )
                
                # Check for other common format string issues
                if 'printf' in line and '%d' in line and 'size_t' in line:
                    self.add_issue(
                        file_path,
                        f"Potential format specifier issue with size_t: {line.strip()}",
                        "warning",
                        i + 1,
                        "format_string",
                        "Use %zu for size_t instead of %d"
                    )
    
    def check_include_path_issues(self):
        """Check for include path issues"""
        self.log("Checking include path issues...")
        
        for file_path, content in self.files.items():
            file_dir = Path(file_path).parent
            
            for i, line in enumerate(content.split('\n')):
                if line.strip().startswith('#include'):
                    # Check for relative includes that might be incorrect
                    if '"' in line and '../' in line:
                        include_match = re.search(r'#include\s*"([^"]+)"', line)
                        if include_match:
                            include_path = include_match.group(1)
                            full_path = file_dir / include_path
                            if not full_path.exists():
                                self.add_issue(
                                    file_path,
                                    f"Potentially incorrect include path: {include_path}",
                                    "warning",
                                    i + 1,
                                    "include_path",
                                    f"Verify include path exists: {full_path}"
                                )
    
    def check_global_variable_declarations(self):
        """Check for missing global variable declarations"""
        self.log("Checking global variable declarations...")
        
        required_globals = ['g_system_health', 'g_state', 'g_config']
        
        for file_path, content in self.files.items():
            for i, line in enumerate(content.split('\n')):
                for required_global in required_globals:
                    if required_global in line and required_global not in self.globals:
                        self.add_issue(
                            file_path,
                            f"Missing global variable declaration: {required_global}",
                            "error",
                            i + 1,
                            "missing_global_declaration",
                            f"Declare global variable {required_global} in appropriate header file"
                        )
    
    def check_compilation_issues(self, directory: str):
        """Run all compilation issue checks"""
        self.log(f"Starting compilation issue check for directory: {directory}")
        
        # Find and load all C files
        c_files = self.find_c_files(directory)
        for file_path in c_files:
            content = self.load_file(file_path)
            if content:
                self.extract_functions(file_path, content)
                self.extract_types(file_path, content)
                self.extract_structs(file_path, content)
                self.extract_globals(file_path, content)
        
        # Run all checks
        self.check_missing_function_implementations()
        self.check_missing_static_function_implementations()
        self.check_missing_type_definitions()
        self.check_missing_struct_members()
        self.check_libubox_compatibility()
        self.check_format_string_issues()
        self.check_include_path_issues()
        self.check_global_variable_declarations()
        
        self.log(f"Compilation issue check complete. Found {len(self.issues)} issues.")
    
    def generate_report(self) -> str:
        """Generate a detailed report of all issues found"""
        if not self.issues:
            return "✅ No compilation issues found!"
        
        # Group issues by severity
        by_severity = defaultdict(list)
        for issue in self.issues:
            by_severity[issue.severity].append(issue)
        
        report = []
        report.append("🔍 Compilation Issues Report")
        report.append("=" * 50)
        report.append("")
        
        # Summary
        total_issues = len(self.issues)
        error_count = len(by_severity.get('error', []))
        warning_count = len(by_severity.get('warning', []))
        
        report.append(f"📊 Summary:")
        report.append(f"  Total issues: {total_issues}")
        report.append(f"  Errors: {error_count}")
        report.append(f"  Warnings: {warning_count}")
        report.append("")
        
        # Detailed results by severity
        severity_order = ['error', 'warning']
        severity_icons = {'error': '❌', 'warning': '⚠️'}
        
        for severity in severity_order:
            if severity in by_severity:
                report.append(f"{severity_icons[severity]} {severity.upper()} ({len(by_severity[severity])})")
                report.append("-" * 30)
                
                for issue in by_severity[severity]:
                    location = f"{issue.file_path}:{issue.line_number}" if issue.line_number else issue.file_path
                    report.append(f"  {location}")
                    report.append(f"    {issue.message}")
                    if issue.suggestion:
                        report.append(f"    💡 {issue.suggestion}")
                    if issue.code_snippet:
                        report.append(f"    📝 {issue.code_snippet}")
                    report.append("")
        
        return "\n".join(report)

def main():
    """Main entry point"""
    import argparse
    
    parser = argparse.ArgumentParser(description="Compilation Issues Checker")
    parser.add_argument("directory", nargs="?", default="src/c", 
                       help="Directory to check (default: src/c)")
    parser.add_argument("-v", "--verbose", action="store_true", 
                       help="Verbose output")
    parser.add_argument("-o", "--output", type=str, 
                       help="Output report to file")
    
    args = parser.parse_args()
    
    # Create checker
    checker = CompilationChecker(verbose=args.verbose)
    
    # Run checks
    checker.check_compilation_issues(args.directory)
    
    # Generate and display report
    report = checker.generate_report()
    print(report)
    
    if args.output:
        with open(args.output, 'w', encoding='utf-8') as f:
            f.write(report)
        print(f"Report saved to {args.output}")
    
    # Exit with appropriate code
    error_count = sum(1 for issue in checker.issues if issue.severity == 'error')
    if error_count > 0:
        sys.exit(1)
    else:
        sys.exit(0)

if __name__ == "__main__":
    main()
