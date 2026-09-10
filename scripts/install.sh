#!/usr/bin/env bash
#
# Install this module on a Move over SSH, without touching the host.
#
# THE MODULE IS THE ONLY THING THIS WRITES. Schwung's own install.sh deploys
# the host and, even with --skip-modules, unpacks modules from its tarball --
# so the order matters when you use both. This one writes exactly one directory.
#
#   scripts/install.sh [--host=IP] [--reload]
#
# --reload restarts Move afterwards, which is REQUIRED for a dsp.so change:
# install alone leaves the previously loaded instance running, so a C fix looks
# like it did not work.
set -euo pipefail

HOST="${MOVE_HOST:-move.local}"
USER_NAME="ableton"
RELOAD=0
for a in "$@"; do
    case "$a" in
        --host=*) HOST="${a#--host=}" ;;
        --reload) RELOAD=1 ;;
        *) echo "unknown option: $a" >&2; exit 2 ;;
    esac
done

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TARBALL="$ROOT/dist/stacks-module.tar.gz"
[ -f "$TARBALL" ] || { echo "no dist/stacks-module.tar.gz -- run scripts/build.sh first" >&2; exit 1; }

DEST="/data/UserData/schwung/modules/midi_fx"
echo "Installing to $USER_NAME@$HOST:$DEST/stacks"
scp -q "$TARBALL" "$USER_NAME@$HOST:/data/UserData/stacks-module.tar.gz"
ssh "$USER_NAME@$HOST" "mkdir -p '$DEST' && tar -xzf /data/UserData/stacks-module.tar.gz -C '$DEST' && rm -f /data/UserData/stacks-module.tar.gz && ls -l '$DEST/stacks'"

if [ "$RELOAD" = "1" ]; then
    echo "Restarting Move so the new dsp.so is loaded..."
    ssh "$USER_NAME@$HOST" "nohup /data/UserData/schwung/restart-move.sh >/dev/null 2>&1 &" || true
    echo "  (~30s; the web manager answers again when it is back)"
fi
