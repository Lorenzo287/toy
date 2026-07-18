#!/bin/sh
set -eu

SOURCE_ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
PREFIX=${XDG_DATA_HOME:-"$HOME/.local/share"}/toy
BIN_DIR=$HOME/.local/bin
CREATE_LINKS=1

usage() {
    cat <<'EOF'
usage: sh install.sh [--prefix DIR] [--bin-dir DIR] [--no-links]

Install a staged Toy SDK. By default the SDK is copied to
$XDG_DATA_HOME/toy (or $HOME/.local/share/toy) and command links are created
under $HOME/.local/bin.
EOF
}

while [ "$#" -gt 0 ]; do
    case "$1" in
        --prefix)
            [ "$#" -ge 2 ] || { echo "install.sh: --prefix requires a directory" >&2; exit 2; }
            PREFIX=$2
            shift 2
            ;;
        --prefix=*)
            PREFIX=${1#--prefix=}
            shift
            ;;
        --bin-dir)
            [ "$#" -ge 2 ] || { echo "install.sh: --bin-dir requires a directory" >&2; exit 2; }
            BIN_DIR=$2
            shift 2
            ;;
        --bin-dir=*)
            BIN_DIR=${1#--bin-dir=}
            shift
            ;;
        --no-links)
            CREATE_LINKS=0
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "install.sh: unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
done

[ -n "$PREFIX" ] || {
    echo "install.sh: --prefix must not be empty" >&2
    exit 2
}
[ -n "$BIN_DIR" ] || {
    echo "install.sh: --bin-dir must not be empty" >&2
    exit 2
}

[ -x "$SOURCE_ROOT/bin/toy" ] || {
    echo "install.sh: run this installer from a staged Toy SDK" >&2
    echo "install.sh: missing executable $SOURCE_ROOT/bin/toy" >&2
    exit 1
}

mkdir -p "$PREFIX"
PREFIX=$(CDPATH= cd -- "$PREFIX" && pwd -P)
if [ "$PREFIX" != "$SOURCE_ROOT" ]; then
    case "$PREFIX/" in
        "$SOURCE_ROOT"/*)
            echo "install.sh: refusing to install a Toy SDK inside itself: $PREFIX" >&2
            exit 1
            ;;
    esac
    cp -R "$SOURCE_ROOT/." "$PREFIX/"
fi

if [ "$CREATE_LINKS" -eq 1 ]; then
    mkdir -p "$BIN_DIR"
    BIN_DIR=$(CDPATH= cd -- "$BIN_DIR" && pwd -P)
    for tool in toy toyfmt toy-lsp toy-dap toy-bindgen toy-c-package; do
        if [ -x "$PREFIX/bin/$tool" ]; then
            ln -sf "$PREFIX/bin/$tool" "$BIN_DIR/$tool"
        fi
    done
    echo "Installed Toy command links in $BIN_DIR"
    case ":${PATH:-}:" in
        *:"$BIN_DIR":*) ;;
        *) echo "Add $BIN_DIR to PATH to use Toy from a new shell." ;;
    esac
fi

echo "Installed Toy SDK to $PREFIX"
echo "Run 'toy --help' to verify the installation."
