# BML+ Documentation Build Script for PowerShell
# This script provides automation for Windows users without Make

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet("install", "install-dev", "build", "serve", "clean", "deploy", "check", "lint", "test", "help")]
    [string]$Task = "help",
    
    [Parameter(Mandatory=$false)]
    [string]$Branch = ""
)

# Colors for output
$Colors = @{
    Red = "Red"
    Green = "Green"
    Yellow = "Yellow"
    Blue = "Blue"
    Cyan = "Cyan"
    White = "White"
    Reset = "White"
}

function Write-ColorOutput {
    param(
        [string]$Message,
        [string]$Color = "White"
    )
    Write-Host $Message -ForegroundColor $Colors[$Color]
}

function Show-Help {
    Write-ColorOutput "BML+ Documentation Build System (PowerShell)" "Cyan"
    Write-ColorOutput "Available tasks:" "Yellow"
    Write-ColorOutput "  install         Install production dependencies" "Blue"
    Write-ColorOutput "  install-dev      Install development dependencies" "Blue"
    Write-ColorOutput "  build           Build the documentation site" "Blue"
    Write-ColorOutput "  serve           Serve documentation locally" "Blue"
    Write-ColorOutput "  clean           Clean build artifacts" "Blue"
    Write-ColorOutput "  deploy          Deploy to GitHub Pages" "Blue"
    Write-ColorOutput "  check           Check documentation for errors" "Blue"
    Write-ColorOutput "  lint            Lint documentation files" "Blue"
    Write-ColorOutput "  test            Run documentation tests" "Blue"
    Write-ColorOutput "  help            Show this help message" "Blue"
    Write-ColorOutput ""
    Write-ColorOutput "Usage:" "Yellow"
    Write-ColorOutput "  .\build.ps1 -Task <taskname>" "White"
    Write-ColorOutput "  .\build.ps1 install" "White"
    Write-ColorOutput "  .\build.ps1 -Task deploy -Branch main" "White"
}

function Install-Dependencies {
    param([bool]$Dev = $false)
    
    Write-ColorOutput "Installing dependencies..." "Green"
    
    if ($Dev) {
        Write-ColorOutput "Installing development dependencies..." "Green"
        pip install -r requirements-dev.txt
        
        Write-ColorOutput "Setting up pre-commit hooks..." "Yellow"
        pre-commit install
    } else {
        Write-ColorOutput "Installing production dependencies..." "Green"
        pip install -r requirements.txt
    }
    
    Write-ColorOutput "Dependencies installed successfully" "Green"
}

function Build-Documentation {
    Write-ColorOutput "Building documentation..." "Green"
    mkdocs build --clean --strict
    Write-ColorOutput "Documentation built successfully in site/" "Green"
}

function Serve-Documentation {
    Write-ColorOutput "Starting local development server..." "Green"
    Write-ColorOutput "Site will be available at http://127.0.0.1:8000" "Yellow"
    mkdocs serve --dev-addr=127.0.0.1:8000
}

function Clean-Artifacts {
    Write-ColorOutput "Cleaning build artifacts..." "Yellow"
    
    if (Test-Path "site") {
        Remove-Item -Recurse -Force "site"
    }
    
    if (Test-Path ".pytest_cache") {
        Remove-Item -Recurse -Force ".pytest_cache"
    }
    
    if (Test-Path ".coverage") {
        Remove-Item -Force ".coverage"
    }
    
    if (Test-Path "htmlcov") {
        Remove-Item -Recurse -Force "htmlcov"
    }
    
    # Clean Python cache files
    Get-ChildItem -Recurse -Directory -Name "__pycache__" | ForEach-Object {
        Remove-Item -Recurse -Force $_
    }
    
    Get-ChildItem -Recurse -File -Filter "*.pyc" | Remove-Item -Force
    Get-ChildItem -Recurse -File -Filter "*.pyo" | Remove-Item -Force
    
    Write-ColorOutput "Clean completed" "Green"
}

function Deploy-Documentation {
    param([string]$Branch = "")
    
    if ($Branch -ne "") {
        Write-ColorOutput "Deploying documentation from branch $Branch..." "Green"
        mkdocs gh-deploy --force --clean --branch $Branch
    } else {
        Write-ColorOutput "Deploying documentation to GitHub Pages..." "Green"
        mkdocs gh-deploy --force --clean
    }
    
    Write-ColorOutput "Documentation deployed successfully" "Green"
}

function Check-Documentation {
    Write-ColorOutput "Checking documentation configuration..." "Green"
    mkdocs build --strict --quiet
    Write-ColorOutput "Documentation check passed" "Green"
}

function Lint-Documentation {
    Write-ColorOutput "Linting documentation files..." "Green"
    markdownlint-cli2 docs/**/*.md
    Write-ColorOutput "Linting completed" "Green"
}

function Run-Tests {
    Write-ColorOutput "Running documentation tests..." "Green"
    python -m pytest tests/ --cov=docs --cov-report=html --cov-report=term
    Write-ColorOutput "Tests completed" "Green"
}

# Main execution
switch ($Task) {
    "help" {
        Show-Help
    }
    "install" {
        Install-Dependencies -Dev $false
    }
    "install-dev" {
        Install-Dependencies -Dev $true
    }
    "build" {
        Build-Documentation
    }
    "serve" {
        Serve-Documentation
    }
    "clean" {
        Clean-Artifacts
    }
    "deploy" {
        Deploy-Documentation -Branch $Branch
    }
    "check" {
        Check-Documentation
    }
    "lint" {
        Lint-Documentation
    }
    "test" {
        Run-Tests
    }
    default {
        Write-ColorOutput "Unknown task: $Task" "Red"
        Show-Help
        exit 1
    }
}