#!/usr/bin/env python3
"""
Version Management Script for Autonomy Project
==============================================

This script helps manage version numbers across all components of the Autonomy project.
It updates the centralized VERSION file and can optionally update git tags.

Usage:
    python3 tools/update_version.py [options] <new_version>
    
Examples:
    python3 tools/update_version.py 1.1.0
    python3 tools/update_version.py 1.0.1 --build 2
    python3 tools/update_version.py 2.0.0 --tag --commit
"""

import os
import sys
import re
import argparse
import subprocess
from pathlib import Path

class VersionManager:
    def __init__(self, project_root=None):
        if project_root is None:
            project_root = Path(__file__).parent.parent
        self.project_root = Path(project_root)
        self.version_file = self.project_root / "VERSION"
        self.version_header = self.project_root / "src" / "c" / "shared" / "autonomy_version.h"
    
    def parse_version(self, version_string):
        """Parse version string into components"""
        # Support formats: "1.0.0", "1.0.0-1", "1.0.0.1"
        version_pattern = r'^(\d+)\.(\d+)\.(\d+)(?:-(\d+))?(?:\.(\d+))?$'
        match = re.match(version_pattern, version_string)
        
        if not match:
            raise ValueError(f"Invalid version format: {version_string}")
        
        major, minor, patch, build, extra = match.groups()
        return {
            'major': int(major),
            'minor': int(minor),
            'patch': int(patch),
            'build': int(build) if build else 1,
            'extra': int(extra) if extra else None
        }
    
    def read_current_version(self):
        """Read current version from VERSION file"""
        if not self.version_file.exists():
            raise FileNotFoundError(f"Version file not found: {self.version_file}")
        
        version_info = {}
        with open(self.version_file, 'r') as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith('#'):
                    if '=' in line:
                        key, value = line.split('=', 1)
                        version_info[key.strip()] = value.strip()
        
        return version_info
    
    def update_version_file(self, new_version, build_number=None):
        """Update the VERSION file with new version"""
        version_components = self.parse_version(new_version)
        
        if build_number is not None:
            version_components['build'] = build_number
        
        version_string = f"{version_components['major']}.{version_components['minor']}.{version_components['patch']}"
        full_version_string = f"{version_string}-{version_components['build']}"
        
        version_content = f"""# Autonomy Project Version Configuration
# This file contains the version information for all packages
# Update this file to change versions across all components

AUTONOMY_VERSION_MAJOR={version_components['major']}
AUTONOMY_VERSION_MINOR={version_components['minor']}
AUTONOMY_VERSION_PATCH={version_components['patch']}
AUTONOMY_VERSION_BUILD={version_components['build']}

# Derived version strings
AUTONOMY_VERSION={version_string}
AUTONOMY_VERSION_FULL={full_version_string}

# Package versions (can be overridden per package if needed)
AUTONOMY_DAEMON_VERSION={full_version_string}
AUTONOMY_API_VERSION={full_version_string}
AUTONOMY_UI_VERSION={full_version_string}
STARLINK_TRACKING_VERSION={full_version_string}
"""
        
        with open(self.version_file, 'w') as f:
            f.write(version_content)
        
        print(f"✅ Updated VERSION file: {new_version}-{version_components['build']}")
    
    def update_version_header(self, new_version, build_number=None):
        """Update the C header file with new version"""
        version_components = self.parse_version(new_version)
        
        if build_number is not None:
            version_components['build'] = build_number
        
        # Read current header file
        with open(self.version_header, 'r') as f:
            content = f.read()
        
        # Update version macros
        content = re.sub(
            r'#define AUTONOMY_VERSION_MAJOR\s+\d+',
            f'#define AUTONOMY_VERSION_MAJOR    {version_components["major"]}',
            content
        )
        content = re.sub(
            r'#define AUTONOMY_VERSION_MINOR\s+\d+',
            f'#define AUTONOMY_VERSION_MINOR    {version_components["minor"]}',
            content
        )
        content = re.sub(
            r'#define AUTONOMY_VERSION_PATCH\s+\d+',
            f'#define AUTONOMY_VERSION_PATCH    {version_components["patch"]}',
            content
        )
        content = re.sub(
            r'#define AUTONOMY_VERSION_BUILD\s+\d+',
            f'#define AUTONOMY_VERSION_BUILD     {version_components["build"]}',
            content
        )
        
        with open(self.version_header, 'w') as f:
            f.write(content)
        
        print(f"✅ Updated version header: {new_version}-{version_components['build']}")
    
    def create_git_tag(self, version, message=None):
        """Create a git tag for the version"""
        if message is None:
            message = f"Release version {version}"
        
        try:
            subprocess.run(['git', 'tag', '-a', f'v{version}', '-m', message], 
                         check=True, cwd=self.project_root)
            print(f"✅ Created git tag: v{version}")
        except subprocess.CalledProcessError as e:
            print(f"❌ Failed to create git tag: {e}")
            return False
        return True
    
    def commit_changes(self, message=None):
        """Commit version changes"""
        if message is None:
            message = "Update version numbers"
        
        try:
            subprocess.run(['git', 'add', 'VERSION', str(self.version_header)], 
                         check=True, cwd=self.project_root)
            subprocess.run(['git', 'commit', '-m', message], 
                         check=True, cwd=self.project_root)
            print(f"✅ Committed changes: {message}")
        except subprocess.CalledProcessError as e:
            print(f"❌ Failed to commit changes: {e}")
            return False
        return True
    
    def show_current_version(self):
        """Show current version information"""
        try:
            version_info = self.read_current_version()
            print("📋 Current Version Information:")
            print(f"  Version: {version_info.get('AUTONOMY_VERSION', 'unknown')}")
            print(f"  Full Version: {version_info.get('AUTONOMY_VERSION_FULL', 'unknown')}")
            print(f"  Major: {version_info.get('AUTONOMY_VERSION_MAJOR', 'unknown')}")
            print(f"  Minor: {version_info.get('AUTONOMY_VERSION_MINOR', 'unknown')}")
            print(f"  Patch: {version_info.get('AUTONOMY_VERSION_PATCH', 'unknown')}")
            print(f"  Build: {version_info.get('AUTONOMY_VERSION_BUILD', 'unknown')}")
        except Exception as e:
            print(f"❌ Error reading version: {e}")

def main():
    parser = argparse.ArgumentParser(description="Manage Autonomy project versions")
    parser.add_argument('version', nargs='?', help='New version number (e.g., 1.1.0)')
    parser.add_argument('--build', type=int, help='Build number (overrides version suffix)')
    parser.add_argument('--tag', action='store_true', help='Create git tag')
    parser.add_argument('--commit', action='store_true', help='Commit changes')
    parser.add_argument('--show', action='store_true', help='Show current version')
    parser.add_argument('--message', help='Custom commit/tag message')
    
    args = parser.parse_args()
    
    manager = VersionManager()
    
    if args.show:
        manager.show_current_version()
        return
    
    if not args.version:
        print("❌ Version number is required (use --show to see current version)")
        sys.exit(1)
    
    try:
        # Update version files
        manager.update_version_file(args.version, args.build)
        manager.update_version_header(args.version, args.build)
        
        # Optional operations
        if args.commit:
            manager.commit_changes(args.message)
        
        if args.tag:
            manager.create_git_tag(args.version, args.message)
        
        print(f"\n🎉 Version update complete!")
        print(f"   New version: {args.version}")
        if args.build:
            print(f"   Build number: {args.build}")
        
    except Exception as e:
        print(f"❌ Error: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()
