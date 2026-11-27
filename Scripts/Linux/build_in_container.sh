#!/usr/bin/env bash
set -euo pipefail
base=$(cd "$(dirname "$0")" && pwd)
docker build -f "$base/Dockerfile.build" -t warlock-ue59-builder "$base"
engine="$base/build/UE-5.9-p2"
docker run --rm --name warlock-ue59-build --cpus=24 --memory=32g \
    --user "$(id -u):$(id -g)" -e HOME="$base/build/container-home" \
    -v "$base:$base" -w "$engine" warlock-ue59-builder \
    bash -c 'source Engine/Build/BatchFiles/Linux/SetupEnvironment.sh -dotnet Engine/Build/BatchFiles/Linux; dotnet Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll WarlockArenaServer Linux Development "$1" -NoUBA -MaxParallelActions=24 -NoDebugInfo' \
    _ "$base/build/project/WarlockArena/WarlockArena.uproject"
