#!/bin/bash

set -e  # salir si hay error

if [ "$#" -lt 4 ]; then
    echo "Uso: $0 <host> <port> <threads> <requests_por_thread> [path]"
    exit 1
fi

HOST="$1"
PORT="$2"
THREADS="$3"
REQUESTS_PER_THREAD="$4"
PATH_REQ="${5:-/}"

echo "Compilando angry_threads_test.c ..."

gcc -Wall -Wextra -O2 ./test/angry_threads_test.c -o ./bin/angry_threads_test

echo "[*] Ejecutando test de carga contra ${HOST}:${PORT}"
echo "    Hilos: ${THREADS}, Requests por hilo: ${REQUESTS_PER_THREAD}, Path: ${PATH_REQ}"
echo

./bin/angry_threads_test "$HOST" "$PORT" "$THREADS" "$REQUESTS_PER_THREAD" "$PATH_REQ"

