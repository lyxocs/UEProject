#!/usr/bin/env bash
set -euo pipefail
base=$(cd "$(dirname "$0")" && pwd)
engine="$base/build/UE-5.9-p2"
for ((i=0;i<180;i++)); do
    if [[ -f "$engine/Engine/Build/OneTimeSetupPerformed" ]]; then
        exec bash "$base/build_server.sh" "$engine" "$base/build/project/WarlockArena"
    fi
    sleep 10
done
echo 'Engine setup did not complete within 30 minutes. Check engine-setup.log.' >&2
exit 1
