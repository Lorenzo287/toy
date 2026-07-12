# install-nvim.ps1
# This script builds Toy tools and installs them to a local directory for Neovim.

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

function Assert-NativeExitCode {
    param(
        [string]$Description,
        [int]$ExitCode
    )

    if ($ExitCode -ne 0) {
        throw "$Description failed with exit code $ExitCode"
    }
}

# Change this path if you prefer a different location
$InstallDir = "C:\toy"

$RepoRoot = (Get-Item $PSScriptRoot).Parent.FullName
$LspSrcDir = Join-Path $RepoRoot "tools\toy-lsp"
$TsSrcDir = Join-Path $RepoRoot "tools\tree-sitter-toy"

Write-Host "--- Toy Local Installation ---" -ForegroundColor Cyan
Write-Host "Target Installation Directory: $InstallDir" -ForegroundColor Gray

# 1. Create Target Directory
if (-not (Test-Path $InstallDir)) {
    Write-Host "Creating installation directory..."
    New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
}

# 2. Generate and Copy Tree-sitter files
Write-Host "`n[1/4] Generating Tree-sitter Parser..." -ForegroundColor Yellow
Push-Location $TsSrcDir
try {
    if (-not (Get-Command npm -ErrorAction SilentlyContinue)) {
        Write-Error "npm is not installed. Please install Node.js/npm."
    }
    
    if (Test-Path -LiteralPath "package-lock.json") {
        Write-Host "Running npm ci..."
        npm ci --silent --ignore-scripts
        Assert-NativeExitCode "npm ci" $LASTEXITCODE
    } else {
        Write-Host "Running npm install..."
        npm install --silent --ignore-scripts
        Assert-NativeExitCode "npm install" $LASTEXITCODE
    }
    npm rebuild tree-sitter-cli --silent
    Assert-NativeExitCode "npm rebuild tree-sitter-cli" $LASTEXITCODE
    
    Write-Host "Generating parser.c and syncing the Go binding..."
    npm run generate --silent
    Assert-NativeExitCode "Tree-sitter parser generation" $LASTEXITCODE

    # Copy the TS folder to the install directory
    # We copy the whole folder because Neovim needs 'src/parser.c' and the 'queries/' directory.
    $TsDest = Join-Path $InstallDir "tree-sitter-toy"
    if (Test-Path -LiteralPath $TsDest) {
        $InstallRoot = [System.IO.Path]::GetFullPath($InstallDir.TrimEnd('\') + '\')
        $TsDestFull = [System.IO.Path]::GetFullPath($TsDest)
        if (-not $TsDestFull.StartsWith($InstallRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
            Write-Error "Refusing to remove unexpected path: $TsDestFull"
        }
        Remove-Item -LiteralPath $TsDestFull -Recurse -Force
    }
    
    # We only need specific subfolders for Neovim to function
    New-Item -ItemType Directory -Path $TsDest -Force | Out-Null
    Copy-Item -LiteralPath "src" -Destination $TsDest -Recurse
    Copy-Item -LiteralPath "queries" -Destination $TsDest -Recurse
    Copy-Item -LiteralPath "grammar.js" -Destination $TsDest
    Copy-Item -LiteralPath "tree-sitter.json" -Destination $TsDest
    
    Write-Host "Tree-sitter files installed to: $TsDest" -ForegroundColor Green
} finally {
    Pop-Location
}

# 3. Build the LSP, DAP, and formatter CLIs after parser.c exists
Write-Host "`n[2/4] Building Toy LSP, DAP, and formatter..." -ForegroundColor Yellow
Push-Location $LspSrcDir
try {
    if (-not (Get-Command go -ErrorAction SilentlyContinue)) {
        Write-Error "Go is not installed. Please install Go to build the LSP."
    }
    Write-Host "Building toy-lsp..."
    go build -o toy-lsp.exe ./cmd/toy-lsp
    Assert-NativeExitCode "toy-lsp build" $LASTEXITCODE
    Write-Host "Building toy-dap..."
    go build -o toy-dap.exe ./cmd/toy-dap
    Assert-NativeExitCode "toy-dap build" $LASTEXITCODE
    Write-Host "Building toyfmt..."
    go build -o toyfmt.exe ./cmd/toyfmt
    Assert-NativeExitCode "toyfmt build" $LASTEXITCODE
    Copy-Item "toy-lsp.exe" -Destination $InstallDir -Force
    Copy-Item "toy-dap.exe" -Destination $InstallDir -Force
    Copy-Item "toyfmt.exe" -Destination $InstallDir -Force
    Write-Host "LSP installed to: $(Join-Path $InstallDir 'toy-lsp.exe')" -ForegroundColor Green
    Write-Host "DAP installed to: $(Join-Path $InstallDir 'toy-dap.exe')" -ForegroundColor Green
    Write-Host "Formatter installed to: $(Join-Path $InstallDir 'toyfmt.exe')" -ForegroundColor Green
} finally {
    Pop-Location
}

# 4. Cleanup old generated Tree-sitter artifacts
Write-Host "`n[3/4] Cleaning up old Tree-sitter artifacts..." -ForegroundColor Yellow
$NvimDataDir = Join-Path $Env:LOCALAPPDATA "nvim-data"
# Remove both pre-rename `toyforth` artifacts and current `toy` artifacts.
$TsParserPaths = @(
    Join-Path $NvimDataDir "site\parser\toyforth.so"
    Join-Path $NvimDataDir "site\parser\toy.so"
    Join-Path $NvimDataDir "lazy\nvim-treesitter\parser\toyforth.so"
    Join-Path $NvimDataDir "lazy\nvim-treesitter\parser\toy.so"
    Join-Path $NvimDataDir "site\pack\packer\start\nvim-treesitter\parser\toyforth.so"
    Join-Path $NvimDataDir "site\pack\packer\start\nvim-treesitter\parser\toy.so"
)

foreach ($Path in $TsParserPaths) {
    if (Test-Path -LiteralPath $Path) {
        try {
            Remove-Item -LiteralPath $Path -Force
            Write-Host "Removed old parser: $Path" -ForegroundColor Green
        } catch {
            Write-Host "Warning: Could not remove $Path. Ensure Neovim is closed." -ForegroundColor Red
        }
    }
}

$TsQueryRoot = Join-Path $NvimDataDir "site\queries"
$TsQueryPaths = @(
    Join-Path $TsQueryRoot "toyforth"
    Join-Path $TsQueryRoot "toy"
)
$TsQueryRootFull = [System.IO.Path]::GetFullPath($TsQueryRoot.TrimEnd('\') + '\')

foreach ($Path in $TsQueryPaths) {
    if (Test-Path -LiteralPath $Path) {
        try {
            $FullPath = [System.IO.Path]::GetFullPath($Path)
            if (-not $FullPath.StartsWith($TsQueryRootFull, [System.StringComparison]::OrdinalIgnoreCase)) {
                Write-Error "Refusing to remove unexpected query path: $FullPath"
            }
            Remove-Item -LiteralPath $FullPath -Recurse -Force
            Write-Host "Removed old queries: $FullPath" -ForegroundColor Green
        } catch {
            Write-Host "Warning: Could not remove $Path. Ensure Neovim is closed." -ForegroundColor Red
        }
    }
}

# 5. Output Neovim Configuration
$LspPathEscaped = (Join-Path $InstallDir "toy-lsp.exe").Replace('\', '\\')
$DapPathEscaped = (Join-Path $InstallDir "toy-dap.exe").Replace('\', '\\')
$TsPathEscaped = (Join-Path $InstallDir "tree-sitter-toy").Replace('\', '/')
$ToyRuntimePath = Join-Path $RepoRoot "build\toy.exe"
$ToyRuntimePathEscaped = $ToyRuntimePath.Replace('\', '\\')
if (-not (Test-Path -LiteralPath $ToyRuntimePath)) {
    Write-Host "Warning: Build Toy before using DAP: $ToyRuntimePath" -ForegroundColor Yellow
}

Write-Host "`n[4/4] --- Neovim Configuration Snippet ---" -ForegroundColor Cyan
Write-Host "Add the following to your init.lua:" -ForegroundColor Gray

$ConfigSnippet = @"
-- Toy LSP Configuration
vim.lsp.config("toyls", {
    cmd = { "$LspPathEscaped" },
    filetypes = { "toy" },
    root_markers = { ".git", "README.md" },
})
vim.lsp.enable("toyls")

-- Register filetypes
vim.filetype.add({
    extension = {
        fth = "toy",
        tf = "toy",
        toy = "toy",
    },
})

-- Toy Tree-sitter Configuration
local function add_toy_parser()
    require("nvim-treesitter.parsers").toy = {
        install_info = {
            path = "$TsPathEscaped",
            queries = "queries/toy",
        },
    }
end

add_toy_parser()
vim.api.nvim_create_autocmd("User", {
    pattern = "TSUpdate",
    callback = add_toy_parser,
})

vim.treesitter.language.register("toy", "toy")

-- Toy Debug Adapter Configuration (requires nvim-dap)
local dap = require("dap")
dap.adapters.toy = {
    type = "executable",
    command = "$DapPathEscaped",
    options = { detached = false },
}
dap.configurations.toy = {
    {
        type = "toy",
        request = "launch",
        name = "Debug current Toy file",
        program = "`${file}",
        cwd = "`${workspaceFolder}",
        runtimeExecutable = "$ToyRuntimePathEscaped",
        stopOnEntry = true,
        args = {},
    },
}
"@

Write-Host "`n$ConfigSnippet" -ForegroundColor White
Write-Host "`nAfter updating your config, restart Neovim and run :TSInstall! toy" -ForegroundColor Yellow
Write-Host "Note: This script attempted to remove any old parser binaries to avoid 'Access is denied' errors." -ForegroundColor Gray
Write-Host "If the error persists, ensure ALL Neovim instances are closed and try again." -ForegroundColor Gray
