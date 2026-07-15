#!/bin/bash
set -e

# install-nvim.sh
# This script builds Toy tools and installs them to a local directory for Neovim (Linux/macOS).

# --- CONFIGURATION ---
# Default installation directory
INSTALL_DIR="${HOME}/.local/share/toy"
# ---------------------

# Get absolute path of the repository root
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LSP_SRC_DIR="${REPO_ROOT}/tools/toy-lsp"
TS_SRC_DIR="${REPO_ROOT}/tools/tree-sitter-toy"

echo -e "\033[0;36m--- Toy Local Installation (Linux/macOS) ---\033[0m"
echo -e "\033[0;90mTarget Installation Directory: ${INSTALL_DIR}\033[0m"

# 1. Create Target Directory
mkdir -p "${INSTALL_DIR}"

# 2. Generate and Copy Tree-sitter files
echo -e "\n\033[0;33m[1/4] Generating Tree-sitter Parser...\033[0m"
if ! command -v npm &> /dev/null; then
    echo "Error: npm is not installed. Please install Node.js/npm."
    exit 1
fi

pushd "${TS_SRC_DIR}" > /dev/null
    if [ -f package-lock.json ]; then
        echo "Running npm ci..."
        npm ci --silent --ignore-scripts
    else
        echo "Running npm install..."
        npm install --silent --ignore-scripts
    fi
    npm rebuild tree-sitter-cli --silent
    
    echo "Generating parser.c and syncing the Go binding..."
    npm run generate --silent

    INSTALL_ROOT="$(cd "${INSTALL_DIR}" && pwd)"
    TS_DEST="${INSTALL_ROOT}/tree-sitter-toy"
    rm -rf -- "${TS_DEST}"
    mkdir -p "${TS_DEST}"
    
    cp -r src queries grammar.js tree-sitter.json "${TS_DEST}/"
    echo -e "\033[0;32mTree-sitter files installed to: ${TS_DEST}\033[0m"
popd > /dev/null

# 3. Build the LSP, DAP, and formatter CLIs after parser.c exists
echo -e "\n\033[0;33m[2/4] Building Toy LSP, DAP, and formatter...\033[0m"
if ! command -v go &> /dev/null; then
    echo "Error: Go is not installed. Please install Go to build the LSP."
    exit 1
fi

pushd "${LSP_SRC_DIR}" > /dev/null
    echo "Building toy-lsp..."
    go build -o toy-lsp ./cmd/toy-lsp
    echo "Building toy-dap..."
    go build -o toy-dap ./cmd/toy-dap
    echo "Building toyfmt..."
    go build -o toyfmt ./cmd/toyfmt
    cp toy-lsp "${INSTALL_DIR}/"
    cp toy-dap "${INSTALL_DIR}/"
    cp toyfmt "${INSTALL_DIR}/"
    echo -e "\033[0;32mLSP installed to: ${INSTALL_DIR}/toy-lsp\033[0m"
    echo -e "\033[0;32mDAP installed to: ${INSTALL_DIR}/toy-dap\033[0m"
    echo -e "\033[0;32mFormatter installed to: ${INSTALL_DIR}/toyfmt\033[0m"
popd > /dev/null

# 4. Cleanup old generated Tree-sitter artifacts
echo -e "\n\033[0;33m[3/4] Cleaning up old Tree-sitter artifacts...\033[0m"
NVIM_DATA_DIR="${XDG_DATA_HOME:-${HOME}/.local/share}/nvim"

# Remove both pre-rename `toyforth` artifacts and current `toy` artifacts.
for parser_path in \
    "${NVIM_DATA_DIR}/site/parser/toyforth.so" \
    "${NVIM_DATA_DIR}/site/parser/toy.so" \
    "${NVIM_DATA_DIR}/lazy/nvim-treesitter/parser/toyforth.so" \
    "${NVIM_DATA_DIR}/lazy/nvim-treesitter/parser/toy.so" \
    "${NVIM_DATA_DIR}/site/pack/packer/start/nvim-treesitter/parser/toyforth.so" \
    "${NVIM_DATA_DIR}/site/pack/packer/start/nvim-treesitter/parser/toy.so"; do
    if [ -e "${parser_path}" ]; then
        rm -f -- "${parser_path}"
        echo -e "\033[0;32mRemoved old parser: ${parser_path}\033[0m"
    fi
done

QUERY_ROOT="${NVIM_DATA_DIR}/site/queries"
for query_name in toyforth toy; do
    QUERY_PATH="${QUERY_ROOT}/${query_name}"
    if [ -e "${QUERY_PATH}" ]; then
        QUERY_ROOT_REAL="$(cd "${QUERY_ROOT}" 2>/dev/null && pwd || true)"
        QUERY_DIR_REAL="$(cd "$(dirname "${QUERY_PATH}")" 2>/dev/null && pwd || true)"
        QUERY_PATH_REAL="${QUERY_DIR_REAL}/$(basename "${QUERY_PATH}")"
        if [ -n "${QUERY_ROOT_REAL}" ] && [ "${QUERY_PATH_REAL#${QUERY_ROOT_REAL}/}" != "${QUERY_PATH_REAL}" ]; then
            rm -rf -- "${QUERY_PATH}"
            echo -e "\033[0;32mRemoved old queries: ${QUERY_PATH_REAL}\033[0m"
        else
            echo "Refusing to remove unexpected query path: ${QUERY_PATH}"
            exit 1
        fi
    fi
done

# 5. Output Neovim Configuration
LSP_PATH="${INSTALL_DIR}/toy-lsp"
DAP_PATH="${INSTALL_DIR}/toy-dap"
TS_PATH="${INSTALL_DIR}/tree-sitter-toy"
TOY_RUNTIME_PATH="${REPO_ROOT}/build/gcc/release/toy"
if [ ! -x "${TOY_RUNTIME_PATH}" ]; then
    echo -e "\033[0;33mWarning: Build Toy before using DAP: ${TOY_RUNTIME_PATH}\033[0m"
fi

echo -e "\n\033[0;36m[4/4] --- Neovim Configuration Snippet ---\033[0m"
echo -e "\033[0;90mAdd the following to your init.lua:\033[0m"

cat <<EOF

-- Toy LSP Configuration
vim.lsp.config("toyls", {
    cmd = { "$LSP_PATH" },
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
            path = "$TS_PATH",
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
    command = "$DAP_PATH",
}
dap.configurations.toy = {
    {
        type = "toy",
        request = "launch",
        name = "Debug current Toy file",
        program = "\${file}",
        cwd = "\${workspaceFolder}",
        runtimeExecutable = "$TOY_RUNTIME_PATH",
        stopOnEntry = true,
        args = {},
    },
}
EOF

echo -e "\n\033[0;33mAfter updating your config, restart Neovim and run :TSInstall! toy\033[0m"
