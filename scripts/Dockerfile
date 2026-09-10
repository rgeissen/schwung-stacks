# Cross-compiler for Ableton Move (aarch64 Linux).
#
# Deliberately minimal: this module is one C file plus a JS UI and an HTML
# panel, so the image needs a cross gcc and nothing else. The Schwung host's
# own builder image carries dbus, systemd, espeak and a Link SDK, none of which
# a MIDI FX has any business linking against.
FROM debian:bookworm

RUN apt-get update && apt-get install -y \
    gcc-aarch64-linux-gnu \
    binutils-aarch64-linux-gnu \
    file \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build
