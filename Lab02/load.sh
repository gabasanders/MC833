#!/usr/bin/env bash
set -euo pipefail

for B in $(seq 0 10); do
  echo "BACKLOG=$B"
  ./servidor 7777 "$B" 10 &
  SVID=$!
  sleep 1  # espera server iniciar


  seq 10 | xargs -P10 -I{} ./cliente 127.0.0.1 7777

  kill "$SVID" 2>/dev/null || true
  wait "$SVID" 2>/dev/null || true

ss -ltnp 'sport = :7777'

# mata servidor
fuser -k 7777/tcp
sleep 3

done