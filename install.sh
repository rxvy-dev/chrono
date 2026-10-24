#!/bin/sh
# Build Chrono and install it (to /usr by default, so login managers list the session).
# Works on Arch, Fedora, Debian/Ubuntu, Void, openSUSE, Alpine and their derivatives.
#
#   ./install.sh                 install dependencies, build, install
#   ./install.sh --no-deps       skip the package manager step (Gentoo, NixOS, or you did it yourself)
#   ./install.sh --prefix /usr/local
#   ./install.sh --uninstall
set -e
cd "$(dirname "$0")"
PREFIX=/usr
DEPS=1
while [ $# -gt 0 ]; do
	case "$1" in
	--no-deps) DEPS=0 ;;
	--prefix) PREFIX="$2"; shift ;;
	--prefix=*) PREFIX="${1#*=}" ;;
	--uninstall)
		[ -f build/install_manifest.txt ] || { echo "nothing to uninstall (no build/install_manifest.txt)"; exit 1; }
		xargs -d '\n' sudo rm -fv < build/install_manifest.txt
		sudo rm -rf "$PREFIX/lib/chrono"
		exit 0 ;;
	-h|--help) sed -n 2,9p "$0"; exit 0 ;;
	*) echo "unknown option $1"; exit 1 ;;
	esac
	shift
done
SUDO=""
[ "$(id -u)" -ne 0 ] && SUDO="sudo"

if [ $DEPS -eq 1 ]; then
	echo "== installing dependencies"
	sh scripts/install-deps.sh || {
		code=$?
		[ $code -eq 2 ] && { echo "install the packages above, then run ./install.sh --no-deps"; exit 1; }
		exit $code
	}
fi

# wlroots 0.20: the distro's package if there is one, else our private copy (built once)
PRIV="$PREFIX/lib/chrono/wlroots"
export PKG_CONFIG_PATH="$PRIV/lib/pkgconfig:$PRIV/share/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
if ! pkg-config --exists wlroots-0.20; then
	echo "== your distro doesn't package wlroots 0.20 yet: building a private copy in $PRIV"
	sh scripts/build-wlroots.sh "$PRIV" "$(pwd)/build/deps"
fi
echo "== using wlroots $(pkg-config --modversion wlroots-0.20) from $(pkg-config --variable=libdir wlroots-0.20)"

GEN=""
if command -v ninja >/dev/null || command -v samu >/dev/null; then GEN="-G Ninja"; fi
cmake -S . -B build $GEN -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX="$PREFIX"
cmake --build build
$SUDO cmake --install build

echo
echo "Chrono is installed. Log out and pick “Chrono” in your login manager, or run chrono-session from a TTY."
