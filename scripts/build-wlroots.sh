#!/bin/sh
# Build wlroots 0.20 into a private prefix for distros that don't package it yet.
# Nothing system-wide is replaced: chrono finds this copy through its RPATH.
# If the distro's libwayland or wayland-protocols is too old for wlroots 0.20 (Debian 13,
# Ubuntu 24.04...), newer copies of those go into the same private prefix as well.
#
#   scripts/build-wlroots.sh PREFIX [WORKDIR]
set -e
PREFIX="${1:?usage: build-wlroots.sh PREFIX [WORKDIR]}"
WORK="${2:-$(pwd)/build/deps}"
SUDO=""
[ "$(id -u)" -ne 0 ] && SUDO="sudo"
mkdir -p "$WORK"
export PKG_CONFIG_PATH="$PREFIX/lib/pkgconfig:$PREFIX/share/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
# everything here finds its siblings in $PREFIX/lib at run time (DT_RPATH is followed by
# the libraries' own dependencies too, unlike DT_RUNPATH)
export LDFLAGS="-Wl,-rpath,$PREFIX/lib -Wl,--disable-new-dtags${LDFLAGS:+ $LDFLAGS}"
export PATH="$PREFIX/bin:$PATH"

latest() {   # latest() URL PATTERN -> newest tag matching PATTERN (e.g. 0.20 -> 0.20.2)
	git ls-remote --tags --refs "$1" | sed 's#.*refs/tags/##' | grep -E "^$2(\.[0-9]+)*$" | sort -V | tail -1
}
required() { # required() DEP -> the minimum version wlroots' meson.build asks for
	grep -oE "dependency\(\s*'$1'[^)]*" "$WORK/wlroots/meson.build" | grep -oE ">= *[0-9.]+" | grep -oE "[0-9.]+" | sort -V | tail -1
}
meson_install() { # meson_install DIR [meson options...]
	d=$1; shift
	(cd "$d" && meson setup build --prefix="$PREFIX" --libdir=lib --buildtype=release "$@" && ninja -C build && $SUDO ninja -C build install)
}

url=https://gitlab.freedesktop.org/wlroots/wlroots.git
tag=$(latest "$url" '0\.20')
[ -n "$tag" ] || tag=0.20.0
echo "== building wlroots $tag into $PREFIX"
rm -rf "$WORK/wlroots"
git clone -q --depth 1 --branch "$tag" "$url" "$WORK/wlroots"

need=$(required wayland-server)
if [ -n "$need" ] && ! pkg-config --atleast-version="$need" wayland-server; then
	wurl=https://gitlab.freedesktop.org/wayland/wayland.git
	wtag=$(latest "$wurl" "$(echo "$need" | cut -d. -f1-2 | sed 's/\./\\./g')")
	echo "== libwayland $(pkg-config --modversion wayland-server 2>/dev/null || echo none) is older than $need: building $wtag privately"
	rm -rf "$WORK/wayland"
	git clone -q --depth 1 --branch "${wtag:-$need}" "$wurl" "$WORK/wayland"
	meson_install "$WORK/wayland" -Ddocumentation=false -Dtests=false -Ddtd_validation=false
fi

need=$(required wayland-protocols)
if [ -n "$need" ] && ! pkg-config --atleast-version="$need" wayland-protocols; then
	purl=https://gitlab.freedesktop.org/wayland/wayland-protocols.git
	echo "== wayland-protocols $(pkg-config --modversion wayland-protocols 2>/dev/null || echo none) is older than $need: building it too"
	rm -rf "$WORK/wayland-protocols"
	git clone -q --depth 1 --branch "$(latest "$purl" '[0-9]+\.[0-9]+' )" "$purl" "$WORK/wayland-protocols"
	meson_install "$WORK/wayland-protocols" -Dtests=false
fi

meson_install "$WORK/wlroots" -Dexamples=false
echo "== wlroots $tag installed in $PREFIX"
