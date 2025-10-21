#!/usr/bin/env bash
set -euo pipefail

# Script simples para disparar várias requisições via ./cliente
# Ajuste as variáveis abaixo conforme necessário.

BACKLOGS=${BACKLOG_VALUES:-"1 5 10"}
REQUESTS=${TOTAL_REQUESTS:-100}
PARALLEL=${CONCURRENCY:-10}
SERVER_SLEEP=${SERVER_SLEEP:-0}

if [[ ! -x ./servidor || ! -x ./cliente ]]; then
  echo "Compile ./servidor e ./cliente antes de rodar este script." >&2
  exit 1
fi

SERVER_PID=""

stop_server() {
  if [[ -n "$SERVER_PID" ]]; then
    kill "$SERVER_PID" 2>/dev/null || true
    wait "$SERVER_PID" 2>/dev/null || true
    SERVER_PID=""
  fi
  rm -f server.info
}
trap stop_server EXIT INT TERM

start_server() {
  local backlog=$1
  rm -f server.info
  ./servidor 0 "$backlog" "$SERVER_SLEEP" &
  SERVER_PID=$!
  while [[ ! -f server.info ]]; do
    sleep 0.05
  done
  awk -F= '
    /^IP=/{ip=$2}
    /^PORT=/{port=$2}
    END {gsub(/\r/,"",ip); gsub(/\r/,"",port); print ip, port}
  ' server.info
}

for backlog in $BACKLOGS; do
  echo "Testando backlog=$backlog"
  read -r ip port < <(start_server "$backlog")
  for ((i = 0; i < REQUESTS; ++i)); do
    ./cliente "$ip" "$port" >/dev/null 2>&1 &
    if (( ((i + 1) % PARALLEL) == 0 )); then
      wait
    fi
  done
  wait
  stop_server
done
