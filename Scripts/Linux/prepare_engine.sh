#!/usr/bin/env bash
set -euo pipefail
archive=$(realpath "$1")
engine=$(realpath -m "$2")
mkdir -p "$engine"
if [[ ! -f "$engine/Engine/Build/Build.version" ]]; then
    tar -xzf "$archive" -C "$engine"
fi
cd "$engine"
# Same licensed Preview-2 source as the Launcher client. Linux build deps only.
./Setup.sh --force --threads=12 --exclude=Win64 --exclude=Win32 --exclude=Mac --exclude=Android --exclude=IOS --exclude=TVOS --exclude=VisionOS --exclude=LinuxArm64
