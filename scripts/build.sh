#!/usr/bin/env bash
#
# Build the Stacks MIDI FX for Move (aarch64) and package it.
#
# Docker by default, because the host toolchain is a cross-compiler nobody has
# lying around; set CROSS_PREFIX to build natively (or from inside the image).
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(dirname "$SCRIPT_DIR")"
IMAGE="schwung-stacks-builder"
ID="stacks"

if [ -z "${CROSS_PREFIX:-}" ] && [ ! -f /.dockerenv ]; then
    if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
        echo "Building the cross-compile image (first time only)..."
        docker build -q -t "$IMAGE" -f "$SCRIPT_DIR/Dockerfile" "$SCRIPT_DIR" >/dev/null
    fi
    exec docker run --rm -v "$ROOT:/build" -u "$(id -u):$(id -g)" \
        -e CROSS_PREFIX=aarch64-linux-gnu- "$IMAGE" \
        /build/scripts/build.sh
fi

cd "$ROOT"
rm -rf "dist/$ID"
mkdir -p "dist/$ID"

# -lpthread: the clip reader runs on a worker thread, because parsing Song.abl
# is file I/O and EVERY MIDI FX ENTRY POINT IS THE SPI CALLBACK.
echo "Compiling dsp.so..."
"${CROSS_PREFIX}gcc" -g -O3 -shared -fPIC \
    src/dsp/stacks.c \
    -o "dist/$ID/dsp.so" \
    -Isrc -lm -lpthread

cp src/module.json src/help.json src/canvas.js src/web_ui.html "dist/$ID/"

# A module tarball unpacks to <id>/, which is what install-module expects.
tar -czf "dist/$ID-module.tar.gz" -C dist "$ID"
echo "Built: dist/$ID-module.tar.gz ($(wc -c < "dist/$ID-module.tar.gz") bytes)"
file "dist/$ID/dsp.so" | sed 's/^/  /'
