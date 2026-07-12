const fs = require("node:fs");
const path = require("node:path");

const repoRoot = path.resolve(__dirname, "..");
const source = path.join(
    repoRoot,
    "tools",
    "tree-sitter-toy",
    "src",
    "parser.c"
);
const destination = path.join(
    repoRoot,
    "tools",
    "toy-lsp",
    "internal",
    "parser",
    "toy",
    "parser.c"
);

if (!fs.existsSync(source)) {
    throw new Error(`generated Tree-sitter parser not found: ${source}`);
}

fs.mkdirSync(path.dirname(destination), { recursive: true });
fs.copyFileSync(source, destination);
console.log("synced generated parser.c into the Go parser package");
