#!/bin/sh
# Build Chrono and install it to /usr (so login managers list the session).
set -e
cd "$(dirname "$0")"
command -v cmake >/dev/null || { echo "install cmake first"; exit 1; }
pkg-config --exists wlroots-0.20 || { echo "wlroots 0.20 is required (Arch: pacman -S wlroots0.20)"; exit 1; }
GEN=""; command -v ninja >/dev/null && GEN="-G Ninja"
cmake -S . -B build $GEN -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
sudo cmake --install build
echo
echo "Chrono is installed. Log out and pick “Chrono” in your login manager, or run chrono-session from a TTY."
