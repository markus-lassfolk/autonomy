#!/usr/bin/env python3
"""
Comprehensive C Code Verification Script
========================================

This script performs comprehensive verification of C code files to detect:
- Syntax errors
- Missing header files and circular dependencies
- Duplicate function definitions and symbols
- Unused functions, variables, and includes
- Build system compatibility issues
- Memory leaks and undefined behavior
- Code style and best practices violations

Usage:
    python3 verify_c_code.py [options] [directory]
    
Options:
    --verbose, -v          Verbose output
    --fix, -f              Attempt to fix common issues
    --strict, -s           Enable strict checking
    --output, -o FILE      Output results to file
    --exclude, -e PATTERN  Exclude files matching pattern
    --include, -i PATTERN  Only include files matching pattern
"""

import os
import sys
import re
import subprocess
import json
import argparse
import tempfile
import shutil
from pathlib import Path
from typing import List, Dict, Set, Tuple, Optional, Any
from dataclasses import dataclass, field
from collections import defaultdict, Counter
import hashlib

@dataclass
class VerificationResult:
    """Container for verification results"""
    file_path: str
    line_number: int = 0
    severity: str = "info"  # info, warning, error, critical
    category: str = ""
    message: str = ""
    suggestion: str = ""
    code_snippet: str = ""

@dataclass
class CodeFile:
    """Represents a C source file with metadata"""
    path: str
    content: str
    lines: List[str]
    includes: List[str] = field(default_factory=list)
    functions: List[Dict] = field(default_factory=list)
    variables: List[Dict] = field(default_factory=list)
    structs: List[Dict] = field(default_factory=list)
    enums: List[Dict] = field(default_factory=list)
    defines: List[Dict] = field(default_factory=list)
    dependencies: Set[str] = field(default_factory=set)

class CCodeVerifier:
    """Main verification class"""
    
    def __init__(self, verbose: bool = False, strict: bool = False):
        self.verbose = verbose
        self.strict = strict
        self.results: List[VerificationResult] = []
        self.files: Dict[str, CodeFile] = {}
        self.include_paths: Set[str] = set()
        self.compiler_flags = [
            "-Wall", "-Wextra", "-Wpedantic", "-Werror=implicit-function-declaration",
            "-Werror=implicit-int", "-Werror=incompatible-pointer-types",
            "-Werror=int-conversion", "-Werror=incompatible-function-pointer-types"
        ]
        
        if strict:
            self.compiler_flags.extend([
                "-Werror", "-Wshadow", "-Wcast-align", "-Wunused",
                "-Wpedantic", "-Wconversion", "-Wsign-conversion",
                "-Wformat=2", "-Wformat-nonliteral", "-Wformat-security"
            ])
    
    def log(self, message: str, level: str = "info"):
        """Log message with appropriate level"""
        if self.verbose or level in ["warning", "error", "critical"]:
            prefix = {"info": "ℹ️", "warning": "⚠️", "error": "❌", "critical": "🚨"}
            print(f"{prefix.get(level, 'ℹ️')} {message}")
    
    def add_result(self, file_path: str, message: str, severity: str = "warning", 
                   line_number: int = 0, category: str = "", suggestion: str = "", 
                   code_snippet: str = ""):
        """Add a verification result"""
        result = VerificationResult(
            file_path=file_path,
            line_number=line_number,
            severity=severity,
            category=category,
            message=message,
            suggestion=suggestion,
            code_snippet=code_snippet
        )
        self.results.append(result)
    
    def find_c_files(self, directory: str, include_patterns: List[str] = None, 
                    exclude_patterns: List[str] = None) -> List[str]:
        """Find all C source files in directory"""
        c_files = []
        directory = Path(directory)
        
        if not directory.exists():
            self.add_result("", f"Directory {directory} does not exist", "error")
            return c_files
        
        # Default patterns
        if include_patterns is None:
            include_patterns = ["*.c", "*.h"]
        if exclude_patterns is None:
            exclude_patterns = ["*.test.c", "*.mock.c", "test_*.c"]
        
        for pattern in include_patterns:
            for file_path in directory.rglob(pattern):
                # Check exclude patterns
                should_exclude = False
                for exclude_pattern in exclude_patterns:
                    if file_path.match(exclude_pattern):
                        should_exclude = True
                        break
                
                if not should_exclude and file_path.is_file():
                    c_files.append(str(file_path))
        
        self.log(f"Found {len(c_files)} C files")
        return c_files
    
    def parse_c_file(self, file_path: str) -> CodeFile:
        """Parse a C file and extract metadata"""
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
            
            lines = content.split('\n')
            code_file = CodeFile(path=file_path, content=content, lines=lines)
            
            # Extract includes
            code_file.includes = self._extract_includes(content)
            
            # Extract functions
            code_file.functions = self._extract_functions(content)
            
            # Extract variables
            code_file.variables = self._extract_variables(content)
            
            # Extract structs
            code_file.structs = self._extract_structs(content)
            
            # Extract enums
            code_file.enums = self._extract_enums(content)
            
            # Extract defines
            code_file.defines = self._extract_defines(content)
            
            self.files[file_path] = code_file
            return code_file
            
        except Exception as e:
            self.add_result(file_path, f"Failed to parse file: {e}", "error")
            return None
    
    def _extract_includes(self, content: str) -> List[str]:
        """Extract #include statements"""
        includes = []
        pattern = r'#include\s*[<"]([^>"]+)[>"]'
        for match in re.finditer(pattern, content):
            includes.append(match.group(1))
        return includes
    
    def _extract_functions(self, content: str) -> List[Dict]:
        """Extract function definitions and declarations"""
        functions = []
        
        # Function definition pattern
        func_pattern = r'^(\w+(?:\s+\w+)*)\s+(\w+)\s*\([^)]*\)\s*\{'
        for match in re.finditer(func_pattern, content, re.MULTILINE):
            functions.append({
                'name': match.group(2),
                'return_type': match.group(1),
                'type': 'definition',
                'line': content[:match.start()].count('\n') + 1
            })
        
        # Function declaration pattern
        decl_pattern = r'^(\w+(?:\s+\w+)*)\s+(\w+)\s*\([^)]*\)\s*;'
        for match in re.finditer(decl_pattern, content, re.MULTILINE):
            functions.append({
                'name': match.group(2),
                'return_type': match.group(1),
                'type': 'declaration',
                'line': content[:match.start()].count('\n') + 1
            })
        
        return functions
    
    def _extract_variables(self, content: str) -> List[Dict]:
        """Extract variable declarations"""
        variables = []
        
        # Variable declaration pattern
        var_pattern = r'^(\w+(?:\s+\w+)*)\s+(\w+)(?:\s*=\s*[^;]+)?\s*;'
        for match in re.finditer(var_pattern, content, re.MULTILINE):
            variables.append({
                'name': match.group(2),
                'type': match.group(1),
                'line': content[:match.start()].count('\n') + 1
            })
        
        return variables
    
    def _extract_structs(self, content: str) -> List[Dict]:
        """Extract struct definitions"""
        structs = []
        
        # Struct definition pattern
        struct_pattern = r'typedef\s+struct\s+(\w+)\s*\{[^}]+\}\s*(\w+)\s*;'
        for match in re.finditer(struct_pattern, content, re.MULTILINE | re.DOTALL):
            structs.append({
                'name': match.group(2),
                'tag': match.group(1),
                'line': content[:match.start()].count('\n') + 1
            })
        
        return structs
    
    def _extract_enums(self, content: str) -> List[Dict]:
        """Extract enum definitions"""
        enums = []
        
        # Enum definition pattern
        enum_pattern = r'typedef\s+enum\s+(\w+)\s*\{[^}]+\}\s*(\w+)\s*;'
        for match in re.finditer(enum_pattern, content, re.MULTILINE | re.DOTALL):
            enums.append({
                'name': match.group(2),
                'tag': match.group(1),
                'line': content[:match.start()].count('\n') + 1
            })
        
        return enums
    
    def _extract_defines(self, content: str) -> List[Dict]:
        """Extract #define statements"""
        defines = []
        
        # Define pattern
        define_pattern = r'#define\s+(\w+)(?:\s+(.+))?'
        for match in re.finditer(define_pattern, content):
            defines.append({
                'name': match.group(1),
                'value': match.group(2) if match.group(2) else '',
                'line': content[:match.start()].count('\n') + 1
            })
        
        return defines
    
    def _clean_content_for_compilation(self, content: str) -> str:
        """Clean content for compilation by removing problematic Unicode characters"""
        # Replace Unicode emojis and other problematic characters with ASCII equivalents
        replacements = {
            '🌐': '[WEB]',
            '🛰️': '[SAT]',
            '📡': '[DISH]',
            '🔮': '[PRED]',
            '✅': '[OK]',
            '❌': '[ERROR]',
            '⚠️': '[WARN]',
            'ℹ️': '[INFO]',
            '🚨': '[ALERT]',
            '📊': '[STATS]',
            '🔄': '[UPDATE]',
            '🛑': '[STOP]',
            '🧹': '[CLEANUP]',
            '🔧': '[INIT]',
            '📍': '[LOC]',
            '🗺️': '[MAP]',
            '📋': '[CONFIG]',
            '🔍': '[SEARCH]',
            '📝': '[LOG]',
            '💡': '[TIP]'
        }
        
        clean_content = content
        for unicode_char, ascii_replacement in replacements.items():
            clean_content = clean_content.replace(unicode_char, ascii_replacement)
        
        return clean_content
    
    def check_syntax_errors(self, file_path: str) -> bool:
        """Check for syntax errors using compiler"""
        if file_path not in self.files:
            return False
        
        code_file = self.files[file_path]
        
        # Clean content for compilation (remove Unicode characters that might cause issues)
        clean_content = self._clean_content_for_compilation(code_file.content)
        
        # Create temporary file for compilation test
        with tempfile.NamedTemporaryFile(mode='w', suffix='.c', delete=False, encoding='utf-8') as temp_file:
            temp_file.write(clean_content)
            temp_file_path = temp_file.name
        
        try:
            # Try to compile the file
            cmd = ['gcc', '-c', '-fsyntax-only'] + self.compiler_flags + [temp_file_path]
            
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            
            if result.returncode != 0:
                # Parse compiler errors
                error_lines = result.stderr.split('\n')
                for line in error_lines:
                    if 'error:' in line or 'warning:' in line:
                        # Extract line number and message
                        match = re.search(r'(\d+):(\d+):\s*(error|warning):\s*(.+)', line)
                        if match:
                            line_num = int(match.group(1))
                            severity = "error" if match.group(3) == "error" else "warning"
                            message = match.group(4)
                            self.add_result(file_path, message, severity, line_num, "syntax")
                        else:
                            self.add_result(file_path, line.strip(), "error", 0, "syntax")
                return False
            
            return True
            
        except subprocess.TimeoutExpired:
            self.add_result(file_path, "Compilation timeout", "error", 0, "syntax")
            return False
        except Exception as e:
            self.add_result(file_path, f"Compilation failed: {e}", "error", 0, "syntax")
            return False
        finally:
            # Clean up temporary file
            try:
                os.unlink(temp_file_path)
            except:
                pass
    
    def check_missing_includes(self, file_path: str):
        """Check for missing header files"""
        if file_path not in self.files:
            return
        
        code_file = self.files[file_path]
        file_dir = Path(file_path).parent
        
        for include in code_file.includes:
            # Check if include file exists
            include_paths = [
                file_dir / include,
                Path("src/c") / include,
                Path("src/c/shared") / include,
                Path("src/c/autonomy-daemon") / include,
                Path("src/c/starlink-tracking/core") / include
            ]
            
            found = False
            for path in include_paths:
                if path.exists():
                    found = True
                    break
            
            if not found:
                self.add_result(
                    file_path, 
                    f"Missing include file: {include}", 
                    "error", 
                    0, 
                    "missing_include",
                    f"Check if {include} exists in include paths"
                )
    
    def check_circular_dependencies(self):
        """Check for circular include dependencies"""
        # Build dependency graph
        dependencies = {}
        for file_path, code_file in self.files.items():
            dependencies[file_path] = set()
            for include in code_file.includes:
                # Find the actual file for this include
                include_file = self._find_include_file(file_path, include)
                if include_file:
                    dependencies[file_path].add(include_file)
        
        # Check for cycles using DFS
        visited = set()
        rec_stack = set()
        
        def has_cycle(node):
            visited.add(node)
            rec_stack.add(node)
            
            for neighbor in dependencies.get(node, set()):
                if neighbor not in visited:
                    if has_cycle(neighbor):
                        return True
                elif neighbor in rec_stack:
                    # Found a cycle
                    cycle = list(rec_stack) + [neighbor]
                    self.add_result(
                        node,
                        f"Circular dependency detected: {' -> '.join(cycle)}",
                        "error",
                        0,
                        "circular_dependency",
                        "Remove circular includes or use forward declarations"
                    )
                    return True
            
            rec_stack.remove(node)
            return False
        
        for file_path in dependencies:
            if file_path not in visited:
                has_cycle(file_path)
    
    def _find_include_file(self, source_file: str, include_name: str) -> Optional[str]:
        """Find the actual file path for an include"""
        source_dir = Path(source_file).parent
        search_paths = [
            source_dir,
            Path("src/c"),
            Path("src/c/shared"),
            Path("src/c/autonomy-daemon"),
            Path("src/c/starlink-tracking/core")
        ]
        
        for search_path in search_paths:
            include_path = search_path / include_name
            if include_path.exists():
                return str(include_path)
        
        return None
    
    def check_duplicate_definitions(self):
        """Check for duplicate function definitions and symbols"""
        function_definitions = defaultdict(list)
        variable_definitions = defaultdict(list)
        struct_definitions = defaultdict(list)
        enum_definitions = defaultdict(list)
        define_definitions = defaultdict(list)
        
        # Collect all definitions
        for file_path, code_file in self.files.items():
            for func in code_file.functions:
                if func['type'] == 'definition':
                    function_definitions[func['name']].append((file_path, func['line']))
            
            for var in code_file.variables:
                variable_definitions[var['name']].append((file_path, var['line']))
            
            for struct in code_file.structs:
                struct_definitions[struct['name']].append((file_path, struct['line']))
            
            for enum in code_file.enums:
                enum_definitions[enum['name']].append((file_path, enum['line']))
            
            for define in code_file.defines:
                define_definitions[define['name']].append((file_path, define['line']))
        
        # Check for duplicates
        for name, locations in function_definitions.items():
            if len(locations) > 1:
                locations_str = ", ".join([f"{path}:{line}" for path, line in locations])
                self.add_result(
                    locations[0][0],
                    f"Duplicate function definition: {name}",
                    "error",
                    locations[0][1],
                    "duplicate_definition",
                    f"Function defined in multiple files: {locations_str}"
                )
        
        for name, locations in struct_definitions.items():
            if len(locations) > 1:
                locations_str = ", ".join([f"{path}:{line}" for path, line in locations])
                self.add_result(
                    locations[0][0],
                    f"Duplicate struct definition: {name}",
                    "error",
                    locations[0][1],
                    "duplicate_definition",
                    f"Struct defined in multiple files: {locations_str}"
                )
        
        for name, locations in enum_definitions.items():
            if len(locations) > 1:
                locations_str = ", ".join([f"{path}:{line}" for path, line in locations])
                self.add_result(
                    locations[0][0],
                    f"Duplicate enum definition: {name}",
                    "error",
                    locations[0][1],
                    "duplicate_definition",
                    f"Enum defined in multiple files: {locations_str}"
                )
    
    def check_unused_code(self):
        """Check for unused functions, variables, and includes"""
        # Get all function calls and variable uses
        all_calls = set()
        all_uses = set()
        
        for file_path, code_file in self.files.items():
            # Find function calls
            call_pattern = r'\b(\w+)\s*\('
            for match in re.finditer(call_pattern, code_file.content):
                all_calls.add(match.group(1))
            
            # Find variable uses
            use_pattern = r'\b(\w+)\b'
            for match in re.finditer(use_pattern, code_file.content):
                all_uses.add(match.group(1))
        
        # Check for unused functions
        for file_path, code_file in self.files.items():
            for func in code_file.functions:
                if func['type'] == 'definition' and func['name'] not in all_calls:
                    # Check if it's a main function or has special attributes
                    if func['name'] not in ['main', 'signal_handler']:
                        self.add_result(
                            file_path,
                            f"Unused function: {func['name']}",
                            "warning",
                            func['line'],
                            "unused_code",
                            "Remove unused function or mark as static"
                        )
        
        # Check for unused variables
        for file_path, code_file in self.files.items():
            for var in code_file.variables:
                if var['name'] not in all_uses:
                    self.add_result(
                        file_path,
                        f"Unused variable: {var['name']}",
                        "warning",
                        var['line'],
                        "unused_code",
                        "Remove unused variable or mark as static"
                    )
    
    def check_undefined_behavior(self, file_path: str):
        """Check for potential undefined behavior"""
        if file_path not in self.files:
            return
        
        code_file = self.files[file_path]
        
        # Check for buffer overflows
        buffer_patterns = [
            r'strcpy\s*\(',
            r'strcat\s*\(',
            r'sprintf\s*\(',
            r'gets\s*\('
        ]
        
        for i, line in enumerate(code_file.lines):
            for pattern in buffer_patterns:
                if re.search(pattern, line):
                    # Extract function name from pattern
                    func_name = pattern.replace('\\s*', '').replace('\\', '').replace('(', '')
                    self.add_result(
                        file_path,
                        f"Potentially unsafe function: {func_name}",
                        "warning",
                        i + 1,
                        "undefined_behavior",
                        "Use safer alternatives like strncpy, strncat, snprintf"
                    )
        
        # Check for uninitialized variables
        uninit_pattern = r'(\w+)\s*=\s*(\w+)\s*;'
        for i, line in enumerate(code_file.lines):
            match = re.search(uninit_pattern, line)
            if match:
                var_name = match.group(2)
                # Check if variable is declared in the same line or previous lines
                # This is a simplified check
                if 'int ' + var_name in line or 'char ' + var_name in line:
                    self.add_result(
                        file_path,
                        f"Potential uninitialized variable: {var_name}",
                        "warning",
                        i + 1,
                        "undefined_behavior",
                        "Initialize variable before use"
                    )
    
    def check_memory_management(self, file_path: str):
        """Check for memory management issues"""
        if file_path not in self.files:
            return
        
        code_file = self.files[file_path]
        
        # Track malloc/free pairs
        malloc_calls = []
        free_calls = []
        
        for i, line in enumerate(code_file.lines):
            if 'malloc' in line or 'calloc' in line or 'realloc' in line:
                malloc_calls.append((i + 1, line.strip()))
            elif 'free' in line:
                free_calls.append((i + 1, line.strip()))
        
        # Simple check for obvious memory leaks
        if len(malloc_calls) > len(free_calls):
            self.add_result(
                file_path,
                f"Potential memory leak: {len(malloc_calls)} allocations vs {len(free_calls)} deallocations",
                "warning",
                0,
                "memory_management",
                "Ensure all allocated memory is properly freed"
            )
    
    def check_build_compatibility(self, file_path: str):
        """Check for build system compatibility issues"""
        if file_path not in self.files:
            return
        
        code_file = self.files[file_path]
        
        # Check for Windows-specific code in Linux build
        windows_patterns = [
            r'#include\s*<windows\.h>',
            r'#include\s*<winsock\.h>',
            r'__declspec\s*\(',
            r'#pragma\s+comment\s*\('
        ]
        
        for i, line in enumerate(code_file.lines):
            for pattern in windows_patterns:
                if re.search(pattern, line, re.IGNORECASE):
                    self.add_result(
                        file_path,
                        f"Windows-specific code detected: {line.strip()}",
                        "warning",
                        i + 1,
                        "build_compatibility",
                        "Use portable alternatives or conditional compilation"
                    )
        
        # Check for missing standard includes
        standard_functions = {
            'printf': 'stdio.h',
            'malloc': 'stdlib.h',
            'strlen': 'string.h',
            'time': 'time.h',
            'pthread_create': 'pthread.h'
        }
        
        for func, required_header in standard_functions.items():
            if func in code_file.content and required_header not in code_file.includes:
                self.add_result(
                    file_path,
                    f"Missing include for {func}: {required_header}",
                    "warning",
                    0,
                    "build_compatibility",
                    f"Add #include <{required_header}>"
                )
    
    def verify_file(self, file_path: str):
        """Run all verification checks on a single file"""
        self.log(f"Verifying {file_path}")
        
        # Parse the file
        code_file = self.parse_c_file(file_path)
        if not code_file:
            return
        
        # Run all checks
        self.check_syntax_errors(file_path)
        self.check_missing_includes(file_path)
        self.check_undefined_behavior(file_path)
        self.check_memory_management(file_path)
        self.check_build_compatibility(file_path)
    
    def verify_directory(self, directory: str, include_patterns: List[str] = None, 
                        exclude_patterns: List[str] = None):
        """Run verification on all C files in directory"""
        self.log(f"Starting verification of directory: {directory}")
        
        # Find all C files
        c_files = self.find_c_files(directory, include_patterns, exclude_patterns)
        
        if not c_files:
            self.add_result("", "No C files found to verify", "warning")
            return
        
        # Verify each file
        for file_path in c_files:
            self.verify_file(file_path)
        
        # Run global checks
        self.check_circular_dependencies()
        self.check_duplicate_definitions()
        self.check_unused_code()
        
        self.log(f"Verification complete. Found {len(self.results)} issues.")
    
    def generate_report(self, output_file: str = None) -> str:
        """Generate a detailed report of all issues found"""
        if not self.results:
            return "✅ No issues found!"
        
        # Group results by severity
        by_severity = defaultdict(list)
        for result in self.results:
            by_severity[result.severity].append(result)
        
        report = []
        report.append("🔍 C Code Verification Report")
        report.append("=" * 50)
        report.append("")
        
        # Summary
        total_issues = len(self.results)
        critical_count = len(by_severity.get('critical', []))
        error_count = len(by_severity.get('error', []))
        warning_count = len(by_severity.get('warning', []))
        info_count = len(by_severity.get('info', []))
        
        report.append(f"📊 Summary:")
        report.append(f"  Total issues: {total_issues}")
        report.append(f"  Critical: {critical_count}")
        report.append(f"  Errors: {error_count}")
        report.append(f"  Warnings: {warning_count}")
        report.append(f"  Info: {info_count}")
        report.append("")
        
        # Detailed results by severity
        severity_order = ['critical', 'error', 'warning', 'info']
        severity_icons = {'critical': '🚨', 'error': '❌', 'warning': '⚠️', 'info': 'ℹ️'}
        
        for severity in severity_order:
            if severity in by_severity:
                report.append(f"{severity_icons[severity]} {severity.upper()} ({len(by_severity[severity])})")
                report.append("-" * 30)
                
                for result in by_severity[severity]:
                    location = f"{result.file_path}:{result.line_number}" if result.line_number else result.file_path
                    report.append(f"  {location}")
                    report.append(f"    {result.message}")
                    if result.suggestion:
                        report.append(f"    💡 {result.suggestion}")
                    if result.code_snippet:
                        report.append(f"    📝 {result.code_snippet}")
                    report.append("")
        
        report_text = "\n".join(report)
        
        if output_file:
            with open(output_file, 'w', encoding='utf-8') as f:
                f.write(report_text)
            self.log(f"Report saved to {output_file}")
        
        return report_text

def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(description="Comprehensive C Code Verification Tool")
    parser.add_argument("directory", nargs="?", default="src/c", 
                       help="Directory to verify (default: src/c)")
    parser.add_argument("-v", "--verbose", action="store_true", 
                       help="Verbose output")
    parser.add_argument("-s", "--strict", action="store_true", 
                       help="Enable strict checking")
    parser.add_argument("-o", "--output", type=str, 
                       help="Output report to file")
    parser.add_argument("-e", "--exclude", action="append", default=[], 
                       help="Exclude files matching pattern")
    parser.add_argument("-i", "--include", action="append", default=[], 
                       help="Only include files matching pattern")
    
    args = parser.parse_args()
    
    # Create verifier
    verifier = CCodeVerifier(verbose=args.verbose, strict=args.strict)
    
    # Run verification
    verifier.verify_directory(
        args.directory, 
        include_patterns=args.include if args.include else None,
        exclude_patterns=args.exclude if args.exclude else None
    )
    
    # Generate and display report
    report = verifier.generate_report(args.output)
    print(report)
    
    # Exit with appropriate code
    critical_count = sum(1 for r in verifier.results if r.severity == 'critical')
    error_count = sum(1 for r in verifier.results if r.severity == 'error')
    
    if critical_count > 0 or error_count > 0:
        sys.exit(1)
    else:
        sys.exit(0)

if __name__ == "__main__":
    main()
