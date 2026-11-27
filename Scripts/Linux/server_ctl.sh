#!/usr/bin/env bash
set -euo pipefail
base=$(cd "$(dirname "$0")" && pwd)
exe="$base/runtime/WarlockArena/Binaries/Linux/WarlockArenaServer"
pidfile="$base/server.pid"
owned() {
    [[ -f "$pidfile" ]] || return 1
    pid=$(cat "$pidfile")
    [[ "$pid" =~ ^[0-9]+$ && "$(readlink -f "/proc/$pid/exe" 2>/dev/null)" == "$exe" ]]
}
case "${1:-status}" in
start)
    if owned; then echo "Server already running: $pid"; exit 0; fi
    [[ -x "$exe" ]] || { echo 'Stage the compiled Linux server first.' >&2; exit 1; }
    mkdir -p "$base/logs"
    nohup bash "$base/run_server.sh" > "$base/logs/stdout.log" 2>&1 < /dev/null &
    echo $! > "$pidfile"
    sleep 2
    owned || { tail -30 "$base/logs/stdout.log"; exit 1; }
    echo "Server running: $pid"
    ;;
stop)
    if owned; then
        kill -TERM "$pid"; echo "Sent TERM to owned server: $pid"
        for ((i=0;i<60;i++)); do
            if ! owned; then exit 0; fi
            sleep .1
        done
        echo 'Server is still shutting down; check status before restaging.' >&2; exit 1
    else echo 'No owned server running'; fi
    ;;
status)
    if owned; then echo "Server running: $pid"; else echo 'Server stopped'; exit 1; fi
    ;;
*) echo 'Usage: server_ctl.sh start|stop|status' >&2; exit 2;;
esac
