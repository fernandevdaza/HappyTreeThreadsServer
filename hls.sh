#!/bin/bash

set -e  # salir si hay error

if [ "$#" -lt 6 ]; then
  echo "Uso: $0 <host> <port> <threads> <iterations_por_thread> <playlist_path> <segments_por_sesion>"
  echo "Ejemplo: $0 127.0.0.1 8000 8 50 /hls/movie1/index.m3u8 20"
  exit 1
fi

HOST="$1"
PORT="$2"
THREADS="$3"
ITERATIONS_PER_THREAD="$4"
PLAYLIST_PATH="$5"
SEGMENTS_PER_SESSION="$6"

echo "Compilando hls_stress_test.c ..."

mkdir -p ./bin

gcc -Wall -Wextra -O2 -pthread ./test/angry_threads_hsl_test.c -o ./bin/hls_stress_test

echo "[*] Ejecutando HLS stress test contra ${HOST}:${PORT}"
echo "    Hilos: ${THREADS}"
echo "    Iteraciones por hilo: ${ITERATIONS_PER_THREAD}"
echo "    Playlist: ${PLAYLIST_PATH}"
echo "    Segmentos por sesión: ${SEGMENTS_PER_SESSION}"
echo

./bin/hls_stress_test "$HOST" "$PORT" "$THREADS" "$ITERATIONS_PER_THREAD" "$PLAYLIST_PATH" "$SEGMENTS_PER_SESSION"
