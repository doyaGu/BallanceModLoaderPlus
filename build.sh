#!/bin/bash
# BML+ Documentation Build Script for Unix/Linux/macOS
# This script provides automation for users without Make

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
WHITE='\033[0;37m'
RESET='\033[0m'

# Variables
PYTHON="${PYTHON:-python3}"
PIP="${PIP:-pip3}"
MKDOCS="${MKDOCS:-mkdocs}"
DOCS_DIR="docs"
SITE_DIR="site"
CONFIG_FILE="mkdocs.yml"
REQUIREMENTS="requirements.txt"
REQUIREMENTS_DEV="requirements-dev.txt"

# Functions
show_help() {
    echo -e "${CYAN}BML+ Documentation Build System (Shell Script)${RESET}"
    echo -e "${YELLOW}Available tasks:${RESET}"
    echo -e "  ${BLUE}install         ${WHITE}Install production dependencies"
    echo -e "  ${BLUE}install-dev      ${WHITE}Install development dependencies"
    echo -e "  ${BLUE}build           ${WHITE}Build the documentation site"
    echo -e "  ${BLUE}serve           ${WHITE}Serve documentation locally"
    echo -e "  ${BLUE}clean           ${WHITE}Clean build artifacts"
    echo -e "  ${BLUE}deploy          ${WHITE}Deploy to GitHub Pages"
    echo -e "  ${BLUE}check           ${WHITE}Check documentation for errors"
    echo -e "  ${BLUE}lint            ${WHITE}Lint documentation files"
    echo -e "  ${BLUE}test            ${WHITE}Run documentation tests"
    echo -e "  ${BLUE}dev-setup       ${WHITE}Complete development setup"
    echo -e "  ${BLUE}help            ${WHITE}Show this help message"
    echo ""
    echo -e "${YELLOW}Usage:${RESET}"
    echo -e "  ${WHITE}./build.sh <taskname>"
    echo -e "  ${WHITE}./build.sh install"
    echo -e "  ${WHITE}./build.sh deploy main"
}

install_dependencies() {
    local dev=${1:-false}
    
    echo -e "${GREEN}Installing dependencies...${RESET}"
    
    if [ "$dev" = true ]; then
        echo -e "${GREEN}Installing development dependencies...${RESET}"
        $PIP install -r $REQUIREMENTS_DEV
        
        echo -e "${YELLOW}Setting up pre-commit hooks...${RESET}"
        pre-commit install
    else
        echo -e "${GREEN}Installing production dependencies...${RESET}"
        $PIP install -r $REQUIREMENTS
    fi
    
    echo -e "${GREEN}Dependencies installed successfully${RESET}"
}

build_documentation() {
    echo -e "${GREEN}Building documentation...${RESET}"
    $MKDOCS build --clean --strict
    echo -e "${GREEN}Documentation built successfully in $SITE_DIR/${RESET}"
}

serve_documentation() {
    echo -e "${GREEN}Starting local development server...${RESET}"
    echo -e "${YELLOW}Site will be available at http://127.0.0.1:8000${RESET}"
    $MKDOCS serve --dev-addr=127.0.0.1:8000
}

clean_artifacts() {
    echo -e "${YELLOW}Cleaning build artifacts...${RESET}"
    
    # Remove build directories
    rm -rf $SITE_DIR/
    rm -rf .pytest_cache/
    rm -rf .coverage
    rm -rf htmlcov/
    
    # Clean Python cache files
    find . -type d -name "__pycache__" -exec rm -rf {} + 2>/dev/null || true
    find . -type f -name "*.pyc" -delete 2>/dev/null || true
    find . -type f -name "*.pyo" -delete 2>/dev/null || true
    
    echo -e "${GREEN}Clean completed${RESET}"
}

deploy_documentation() {
    local branch=${1:-""}
    
    if [ -n "$branch" ]; then
        echo -e "${GREEN}Deploying documentation from branch $branch...${RESET}"
        $MKDOCS gh-deploy --force --clean --branch "$branch"
    else
        echo -e "${GREEN}Deploying documentation to GitHub Pages...${RESET}"
        $MKDOCS gh-deploy --force --clean
    fi
    
    echo -e "${GREEN}Documentation deployed successfully${RESET}"
}

check_documentation() {
    echo -e "${GREEN}Checking documentation configuration...${RESET}"
    $MKDOCS build --strict --quiet
    echo -e "${GREEN}Documentation check passed${RESET}"
}

lint_documentation() {
    echo -e "${GREEN}Linting documentation files...${RESET}"
    markdownlint-cli2 $DOCS_DIR/**/*.md
    echo -e "${GREEN}Linting completed${RESET}"
}

run_tests() {
    echo -e "${GREEN}Running documentation tests...${RESET}"
    $PYTHON -m pytest tests/ --cov=docs --cov-report=html --cov-report=term
    echo -e "${GREEN}Tests completed${RESET}"
}

dev_setup() {
    echo -e "${CYAN}Setting up development environment...${RESET}"
    
    # Create virtual environment if it doesn't exist
    if [ ! -d "venv" ]; then
        echo -e "${YELLOW}Creating virtual environment...${RESET}"
        $PYTHON -m venv venv
        echo -e "${YELLOW}Virtual environment created. Activate with: source venv/bin/activate${RESET}"
    fi
    
    # Install dependencies
    install_dependencies true
    
    echo -e "${GREEN}Development setup completed${RESET}"
}

show_stats() {
    echo -e "${CYAN}Documentation Statistics:${RESET}"
    echo -e "${BLUE}Total markdown files:${RESET} $(find $DOCS_DIR -name "*.md" | wc -l)"
    echo -e "${BLUE}Total lines of documentation:${RESET} $(find $DOCS_DIR -name "*.md" -exec wc -l {} + | tail -1 | awk '{print $1}')"
    echo -e "${BLUE}Site directory size:${RESET} $(du -sh $SITE_DIR 2>/dev/null || echo "Not built yet")"
}

# Main execution
TASK=${1:-"help"}
BRANCH=${2:-""}

case $TASK in
    "help")
        show_help
        ;;
    "install")
        install_dependencies false
        ;;
    "install-dev")
        install_dependencies true
        ;;
    "build")
        build_documentation
        ;;
    "serve")
        serve_documentation
        ;;
    "clean")
        clean_artifacts
        ;;
    "deploy")
        deploy_documentation "$BRANCH"
        ;;
    "check")
        check_documentation
        ;;
    "lint")
        lint_documentation
        ;;
    "test")
        run_tests
        ;;
    "dev-setup")
        dev_setup
        ;;
    "stats")
        show_stats
        ;;
    *)
        echo -e "${RED}Unknown task: $TASK${RESET}"
        show_help
        exit 1
        ;;
esac