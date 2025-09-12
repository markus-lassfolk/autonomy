#!/bin/bash

# Update Project Documentation Script
# This script helps automatically update CHANGELOG.md and PROJECT_STATUS.md

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Project root directory
PROJECT_ROOT="/mnt/s/autonomy"
CHANGELOG_FILE="$PROJECT_ROOT/CHANGELOG.md"
PROJECT_STATUS_FILE="$PROJECT_ROOT/PROJECT_STATUS.md"
VERSION_FILE="$PROJECT_ROOT/VERSION"

echo -e "${BLUE}=== Autonomy Project Documentation Updater ===${NC}"
echo -e "${GREEN}This script ensures proper date/time formatting in documentation${NC}"
echo -e "${YELLOW}Always use this script instead of manual date entry to prevent errors${NC}"
echo -e ""

# Function to get current version
get_current_version() {
    if [ -f "$VERSION_FILE" ]; then
        grep "AUTONOMY_VERSION_FULL=" "$VERSION_FILE" | cut -d'=' -f2
    else
        echo "5.8.4-205"  # Default fallback
    fi
}

# Function to get current date
get_current_date() {
    date +"%Y-%m-%d"
}

# Function to get recent commits
get_recent_commits() {
    cd "$PROJECT_ROOT"
    git log --oneline -10
}

# Function to get current git status
get_git_status() {
    cd "$PROJECT_ROOT"
    git status --porcelain
}

# Function to update changelog with new entry
update_changelog() {
    local version="$1"
    local date="$2"
    local entry_type="$3"  # "Added", "Changed", "Fixed", "Security"
    local description="$4"
    
    echo -e "${YELLOW}Updating CHANGELOG.md...${NC}"
    
    # Create temporary file
    local temp_file=$(mktemp)
    
    # Add header and new entry
    cat > "$temp_file" << EOF
# Changelog

All notable changes to the Autonomy Daemon project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [$version] - $date

### $entry_type
- $description

EOF
    
    # Append existing changelog content (skip header)
    tail -n +12 "$CHANGELOG_FILE" >> "$temp_file"
    
    # Replace original file
    mv "$temp_file" "$CHANGELOG_FILE"
    
    echo -e "${GREEN}✓ CHANGELOG.md updated with new entry${NC}"
}

# Function to update project status
update_project_status() {
    local date="$1"
    
    echo -e "${YELLOW}Updating PROJECT_STATUS.md...${NC}"
    
    # Update the date in the header
    sed -i "s/# Project Status - .*/# Project Status - $date/" "$PROJECT_STATUS_FILE"
    
    echo -e "${GREEN}✓ PROJECT_STATUS.md date updated${NC}"
}

# Function to show current status
show_status() {
    echo -e "${BLUE}=== Current Project Status ===${NC}"
    echo -e "Version: $(get_current_version)"
    echo -e "Date: $(get_current_date)"
    echo -e ""
    echo -e "${BLUE}Recent Commits:${NC}"
    get_recent_commits
    echo -e ""
    echo -e "${BLUE}Git Status:${NC}"
    get_git_status
}

# Function to show help
show_help() {
    echo -e "${BLUE}Usage: $0 [OPTION]${NC}"
    echo -e ""
    echo -e "Options:"
    echo -e "  status          Show current project status"
    echo -e "  changelog       Add new changelog entry"
    echo -e "  update-date     Update date in PROJECT_STATUS.md"
    echo -e "  help            Show this help message"
    echo -e ""
    echo -e "Examples:"
    echo -e "  $0 status"
    echo -e "  $0 changelog"
    echo -e "  $0 update-date"
}

# Main script logic
case "${1:-status}" in
    "status")
        show_status
        ;;
    "changelog")
        version=$(get_current_version)
        date=$(get_current_date)
        echo -e "${YELLOW}Adding new changelog entry for version $version on $date${NC}"
        echo -e "${BLUE}Entry types: Added, Changed, Fixed, Security${NC}"
        echo -e "Enter entry type: "
        read -r entry_type
        echo -e "Enter description: "
        read -r description
        echo -e "${GREEN}Adding entry with automatic date formatting...${NC}"
        update_changelog "$version" "$date" "$entry_type" "$description"
        echo -e "${GREEN}✓ Changelog entry added with proper date/time formatting${NC}"
        ;;
    "update-date")
        date=$(get_current_date)
        echo -e "${YELLOW}Updating PROJECT_STATUS.md date to $date${NC}"
        update_project_status "$date"
        echo -e "${GREEN}✓ PROJECT_STATUS.md date updated with proper formatting${NC}"
        ;;
    "help"|"-h"|"--help")
        show_help
        ;;
    *)
        echo -e "${RED}Unknown option: $1${NC}"
        show_help
        exit 1
        ;;
esac

echo -e "${GREEN}✓ Documentation update complete${NC}"
echo -e "${BLUE}Remember: Always use this script for proper date/time formatting!${NC}"
