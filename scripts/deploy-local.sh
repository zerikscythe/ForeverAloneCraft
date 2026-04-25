#!/usr/bin/env bash
# Copies freshly built server binaries to the local deployment folder.
# The target path is read from deploy.local in the repo root — create that
# file with a single line containing the absolute path to your server folder.
# Example (WSL):  /mnt/e/WoWzers/WotLK/Server
#
# Usage: ./scripts/deploy-local.sh [build-dir]
#   build-dir defaults to ./build

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CONFIG="$REPO_ROOT/deploy.local"

if [ ! -f "$CONFIG" ]; then
    echo "ERROR: $CONFIG not found."
    echo "Create it with one line: the absolute path to your server folder."
    echo "  Example: /mnt/e/WoWzers/WotLK/Server"
    exit 1
fi

DEPLOY_PATH="$(tr -d '[:space:]' < "$CONFIG")"

if [ -z "$DEPLOY_PATH" ]; then
    echo "ERROR: $CONFIG is empty."
    exit 1
fi

if [ ! -d "$DEPLOY_PATH" ]; then
    echo "ERROR: deploy target does not exist or is not mounted: $DEPLOY_PATH"
    exit 1
fi

BUILD_DIR="${1:-$REPO_ROOT/build}"

echo "Deploying from $BUILD_DIR to $DEPLOY_PATH ..."

cmake --install "$BUILD_DIR" --prefix "$DEPLOY_PATH"

echo "Deploy complete."
