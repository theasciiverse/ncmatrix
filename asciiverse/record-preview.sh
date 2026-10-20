#!/bin/sh
# Records the catalog preview at 100x30 (hub/RECORDING.md): ncmatrix as av
# starts it, while a download runs, so the traffic shows in the rain;
# ended by SIGTERM, capped at 20 fps. Needs asciinema 3, curl and uv (for
# pyte).
#
#   asciiverse/record-preview.sh out/ncmatrix asciiverse/ncmatrix.cast [seconds]
set -eu
binary=$(cd "$(dirname "$1")" && pwd)/$(basename "$1")
out=$(pwd)/$2
seconds=${3:-20}
here=$(cd "$(dirname "$0")" && pwd)
home=$(mktemp -d)
trap 'rm -rf "$home"; kill $load 2>/dev/null || true' EXIT
mkdir "$home/bin" && cp "$binary" "$home/bin/ncmatrix"
# some traffic: a few downloads one after another, and back
(sleep 2; for i in 1 2 3 4; do curl -s -o /dev/null 'https://speed.cloudflare.com/__down?bytes=25000000'; sleep 1; done) &
load=$!
cd "$home"  # nothing in the recording names a local path
HOME="$home" PATH="$home/bin:$PATH" TERM=xterm-256color LANG=C.UTF-8 \
asciinema rec --headless --overwrite -q \
  --window-size 100x30 --idle-time-limit 2 \
  --capture-env "TERM,LANG" --title "NCMatrix" \
  --command "timeout --foreground -s TERM $seconds ncmatrix -s -a -b" \
  "$home/raw.cast"
uv run -q --with pyte python -I "$here/cast-cap-fps.py" "$home/raw.cast" "$out" 20
