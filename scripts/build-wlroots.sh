#!/bin/sh
# Build wlroots 0.20 into a private prefix for distros that don't package it yet.
# Nothing system-wide is replaced: chrono finds this copy through its RPATH.
#
#   scripts/build-wlroots.sh PREFIX [WORKDIR]
set -e
PREFIX="${1:?usage: build-wlroots.sh PREFIX [WORKDIR]}"
WORK="${2:-$(pwd)/build/deps}"
SUDO=""
[ "$(id -u)" -ne 0 ] && SUDO="sudo"
mkdir -p "$WORK"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:$PREFIX/share/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

# newest 0.20.x release
url=https://gitlab.freedesktop.org/wlroots/wlroots.git
tag=$(git ls-remote --tags --refs "$url" '0.20.*' | sed 's#.*refs/tags/##' | grep -E '^0\.20\.[0-9]+$' | sort -t. -k3 -n | tail -1)
[ -n "$tag" ] || tag=0.20.0
echo "== building wlroots $tag into $PREFIX"
rm -rf "$WORK/wlroots"
git clone -q --depth 1 --branch "$tag" "$url" "$WORK/wlroots"

# wayland-protocols is only XML + a .pc file, so a newer private copy is safe when ours is too old
need=$(grep -oE "'wayland-protocols',[^)]*version: *'>=([0-9.]+)'" "$WORK/wlroots/meson.build" | grep -oE '[0-9]+\.[0-9]+' | tail -1)
if [ -n "$need" ] && ! pkg-config --atleast-version="$need" wayland-protocols; then
	echo "== wayland-protocols $(pkg-config --modversion wayland-protocols 2>/dev/null || echo none) is older than $need: building it too"
	rm -rf "$WORK/wayland-protocols"
	git clone -q --depth 1 --branch "$need" https://gitlab.freedesktop.org/wayland/wayland-protocols.git "$WORK/wayland-protocols" \
		|| git clone -q --depth 1 https://gitlab.freedesktop.org/wayland/wayland-protocols.git "$WORK/wayland-protocols"
	(cd "$WORK/wayland-protocols" && meson setup build --prefix="$PREFIX" --libdir=lib -Dtests=false >/dev/null && ninja -C build && $SUDO ninja -C build install >/dev/null)
fi

cd "$WORK/wlroots"
meson setup build --prefix="$PREFIX" --libdir=lib --buildtype=release -Dexamples=false
ninja -C build
$SUDO ninja -C build install
echo "== wlroots $tag installed in $PREFIX"
