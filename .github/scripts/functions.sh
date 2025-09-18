#!/bin/bash
# This script is only for GitHub Actions use
set -euo pipefail

KERNEL="$(uname -s)"
MACHINE="$(uname -m)"

export KERNEL MACHINE

export WORKSPACE="${GITHUB_WORKSPACE:-$(pwd)}"
export CURL_VERSION="8.6.0_1"

case "$KERNEL" in
Linux)
    KERNEL="linux"
    ;;
Darwin)
    KERNEL="darwin"
    MACHINE="universal"
    ;;
esac

function copy-license {
    local OUTPUT="$1"

    cp -r "$WORKSPACE/LICENSES" "$OUTPUT"
    cp "$WORKSPACE/REUSE.toml" "$OUTPUT"
}

function copy-usage {
    local OUTPUT="$1"

    cp "$WORKSPACE/docs/USAGE.md" "$OUTPUT/README.md"
}

function create-bundle {
    local BUNDLE_FILE="$1"
    local INPUT_DIR="$2"

    pushd "$INPUT_DIR"
    zip -r "$BUNDLE_FILE" ./*
    popd
}
