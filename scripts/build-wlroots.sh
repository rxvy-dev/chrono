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
	tr '\n' ' ' < "$WORK/wlroots/meson.build" | grep -oE "dependency\([[:space:]]*'$1'[^)]*" | grep -oE ">= *[0-9.]+" | grep -oE "[0-9.]+" | sort -V | tail -1
}
meson_install() { # meson_install DIR [meson options...]
	d=$1; shift
	(cd "$d" && meson setup build --prefix="$PREFIX" --libdir=lib --buildtype=release "$@" && ninja -C build && $SUDO ninja -C build install)
}

# the fallback recipes need a recent meson (Ubuntu 24.04 ships 1.3): use a private one if so
if ! meson --version 2>/dev/null | awk -F. '{ exit !($1 > 1 || ($1 == 1 && $2 >= 4)) }'; then
	echo "== meson $(meson --version 2>/dev/null) is too old: using a newer one from PyPI (private venv)"
	python3 -m venv "$WORK/venv" && "$WORK/venv/bin/pip" install -q meson
	export PATH="$WORK/venv/bin:$PATH"
fi

url=https://gitlab.freedesktop.org/wlroots/wlroots.git
tag=$(latest "$url" '0\.20')
[ -n "$tag" ] || tag=0.20.0
echo "== building wlroots $tag into $PREFIX"
rm -rf "$WORK/wlroots"
git clone -q --depth 1 --branch "$tag" "$url" "$WORK/wlroots"

build_wayland() { # libwayland >= $1, privately
	wurl=https://gitlab.freedesktop.org/wayland/wayland.git
	wtag=$(latest "$wurl" "$(echo "$1" | cut -d. -f1-2 | sed 's/\./\\./g')")
	echo "== libwayland $(pkg-config --modversion wayland-server 2>/dev/null || echo none) is older than $1: building ${wtag:-$1} privately"
	rm -rf "$WORK/wayland"
	git clone -q --depth 1 --branch "${wtag:-$1}" "$wurl" "$WORK/wayland"
	meson_install "$WORK/wayland" -Ddocumentation=false -Dtests=false -Ddtd_validation=false
}
build_protocols() { # wayland-protocols >= $1, privately (just XML + a .pc file)
	purl=https://gitlab.freedesktop.org/wayland/wayland-protocols.git
	echo "== wayland-protocols $(pkg-config --modversion wayland-protocols 2>/dev/null || echo none) is older than $1: building it too"
	rm -rf "$WORK/wayland-protocols"
	git clone -q --depth 1 --branch "$(latest "$purl" '[0-9]+\.[0-9]+')" "$purl" "$WORK/wayland-protocols"
	meson_install "$WORK/wayland-protocols" -Dtests=false
}
# what meson says is too old, e.g. "Found 1.23.1 but need: '>=1.24.0'"
too_old() { grep -iE "Dependency $1 found: NO.*need: '>=" "$WORK/setup.log" | grep -oE ">=[0-9.]+" | tr -d '>=' | tail -1; }

need=$(required wayland-server); [ -n "$need" ] && ! pkg-config --atleast-version="$need" wayland-server && build_wayland "$need"
need=$(required wayland-protocols); [ -n "$need" ] && ! pkg-config --atleast-version="$need" wayland-protocols && build_protocols "$need"
# second chance from meson's own error messages; anything else too old (libdrm on Debian 13...)
# comes from the wrap fallbacks wlroots ships, installed into the same private prefix
for attempt in 1 2 3; do
	rm -rf "$WORK/wlroots/build"
	(cd "$WORK/wlroots" && meson setup build --prefix="$PREFIX" --libdir=lib --buildtype=release -Dexamples=false --wrap-mode=default) >"$WORK/setup.log" 2>&1 && break
	w=$(too_old wayland-server); p=$(too_old wayland-protocols)
	[ -z "$w$p" ] && { cat "$WORK/setup.log"; exit 1; }
	[ -n "$w" ] && build_wayland "$w"
	[ -n "$p" ] && build_protocols "$p"
done
(cd "$WORK/wlroots" && ninja -C build && $SUDO ninja -C build install)
echo "== wlroots $tag installed in $PREFIX"
