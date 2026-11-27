#!/usr/bin/env bash
set -euo pipefail
base=$(cd "$(dirname "$0")" && pwd)
runtime="$base/runtime"
port=${WARLOCK_PORT:-7777}
seats=${WARLOCK_SEATS:-4}
mkdir -p "$base/logs"
cd "$runtime"
exec "$runtime/WarlockArena/Binaries/Linux/WarlockArenaServer" \
    /Game/WarlockArena/Maps/Arena -port="$port" -Participants="$seats" \
    -nullrhi -nosound -unattended -stdout -FullStdOutLogOutput \
    -abslog="$base/logs/WarlockArenaServer.log" "$@"
