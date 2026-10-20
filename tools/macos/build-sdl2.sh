#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
#
# Builds the pinned SDL2 release as one universal (arm64 + x86_64) dylib, so a single .app
# runs on Apple Silicon and Intel Macs. Homebrew's SDL2 is single-architecture and points at
# /opt/homebrew, which a player does not have.
set -euo pipefail

PREFIX=$1
VERSION=2.30.12
SHA256=ac356ea55e8b9dd0b2d1fa27da40ef7e238267ccf9324704850d5d47375b48ea
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

curl -fsSL -o "$WORK/sdl.tar.gz" "https://github.com/libsdl-org/SDL/releases/download/release-$VERSION/SDL2-$VERSION.tar.gz"
echo "$SHA256  $WORK/sdl.tar.gz" | shasum -a 256 -c
tar -C "$WORK" -xzf "$WORK/sdl.tar.gz"
cmake -S "$WORK/SDL2-$VERSION" -B "$WORK/build" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_ARCHITECTURES='arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-11.0}" \
  -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF -DCMAKE_INSTALL_PREFIX="$PREFIX"
cmake --build "$WORK/build" --parallel 3
cmake --install "$WORK/build"
cp "$WORK/SDL2-$VERSION/LICENSE.txt" "$PREFIX/SDL2-license.txt"
lipo -archs "$PREFIX/lib/libSDL2-2.0.0.dylib"
