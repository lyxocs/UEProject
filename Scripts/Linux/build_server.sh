#!/usr/bin/env bash
set -euo pipefail
engine=$(realpath "$1")
project=$(realpath "$2")
"$engine/Engine/Build/BatchFiles/BuildUBT.sh"
"$engine/Engine/Build/BatchFiles/Linux/Build.sh" WarlockArenaServer Linux Development "$project/WarlockArena.uproject" -NoUBA -MaxParallelActions=6 -NoDebugInfo
