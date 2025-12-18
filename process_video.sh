#!/bin/bash

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 <video_path> <output_path>"
  exit 1
fi


INPUT="$1"
OUTPUT="$2"

ffmpeg -i "$INPUT" \
  -c:v libx264 -crf 20 -g 48 -keyint_min 48 -sc_threshold 0 \
  -c:a aac -b:a 128k \
  -hls_time 1 -hls_playlist_type vod \
  -hls_segment_type fmp4 \
  -hls_segment_filename "${OUTPUT}/seg_%05d.ts" \
  "${OUTPUT}/index.m3u8"
