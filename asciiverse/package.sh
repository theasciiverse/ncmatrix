#!/bin/sh
# Packs the build into dist/ncmatrix-<target>.tar.gz (.zip for Windows) for
# av: the binary at the top beside its man page, README.md, COPYING (GPL
# 3), NOTICE (who made it) and PATCHES.md.
#
#   asciiverse/package.sh x86_64-unknown-linux-musl
set -eu
cd "$(dirname "$0")/.."
target="$1"
case "$target" in *windows*) exe=.exe ;; *) exe= ;; esac
stage=stage/ncmatrix
rm -rf "$stage" && mkdir -p "$stage" dist
cp "out/ncmatrix$exe" README.md COPYING NOTICE PATCHES.md "$stage/"
cp cmatrix.1 "$stage/ncmatrix.1"
if [ -n "$exe" ]; then
  (cd "$stage" && zip -qr "../../dist/ncmatrix-$target.zip" .)
else
  tar -czf "dist/ncmatrix-$target.tar.gz" -C "$stage" .
fi
ls dist
