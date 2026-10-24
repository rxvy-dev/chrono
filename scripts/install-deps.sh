#!/bin/sh
# Install everything Chrono needs to build and run, using the distro's own package manager.
# Used by install.sh and by CI (so the lists below are tested on every supported distro).
#
#   scripts/install-deps.sh            install build + runtime packages
#   scripts/install-deps.sh --print    just print the command for this distro
#
# wlroots 0.20 is installed from packages when the distro has it; otherwise install.sh builds
# a private copy (scripts/build-wlroots.sh), so an older system wlroots is never touched.
set -e

SUDO=""
[ "$(id -u)" -ne 0 ] && SUDO="sudo"
PRINT=0
[ "$1" = "--print" ] && PRINT=1

. /etc/os-release 2>/dev/null || { echo "can't read /etc/os-release"; exit 1; }
family=""
for id in $ID $ID_LIKE; do
	case "$id" in
	arch|artix|endeavouros|manjaro|cachyos|garuda) family=arch ;;
	fedora|rhel|centos|nobara|ultramarine) family=fedora ;;
	debian|ubuntu|linuxmint|pop|elementary|zorin|kali|raspbian|neon) family=debian ;;
	void) family=void ;;
	opensuse*|suse|sles) family=suse ;;
	alpine|postmarketos) family=alpine ;;
	gentoo) family=gentoo ;;
	nixos) family=nixos ;;
	esac
	[ -n "$family" ] && break
done

run() {
	if [ $PRINT -eq 1 ]; then echo "$*"; else echo "+ $*"; $SUDO "$@"; fi
}
# install the packages that exist, skipping ones this release doesn't have (optional extras)
try() {
	[ $PRINT -eq 1 ] && { echo "(optional) $*"; return 0; }
	$SUDO "$@" >/dev/null 2>&1 || echo "  (optional, not available here: ${*##* })"
}

case "$family" in
arch)
	run pacman -S --needed --noconfirm base-devel git cmake meson ninja pkgconf \
		wayland wayland-protocols libinput libxkbcommon pixman libdrm cairo pango pam \
		xcb-util-wm xcb-util-renderutil xcb-util-errors libxcb xorg-xwayland seatd \
		libdisplay-info libliftoff hwdata vulkan-headers glslang mesa lcms2 \
		qt6-base qt6-declarative qt6-wayland qt6-svg layer-shell-qt \
		dbus grim slurp wl-clipboard polkit
	try pacman -S --needed --noconfirm wlroots0.20
	for p in foot xdg-desktop-portal-wlr xdg-desktop-portal-gtk brightnessctl playerctl networkmanager; do
		try pacman -S --needed --noconfirm $p
	done
	;;
fedora)
	# dnf understands pkgconfig(...)/cmake(...) names, which don't change between releases
	run dnf install -y gcc gcc-c++ git cmake meson ninja-build pkgconf-pkg-config \
		'pkgconfig(wayland-server)' 'pkgconfig(wayland-protocols)' 'pkgconfig(libinput)' \
		'pkgconfig(xkbcommon)' 'pkgconfig(pixman-1)' 'pkgconfig(libdrm)' 'pkgconfig(cairo)' \
		'pkgconfig(pangocairo)' pam-devel 'pkgconfig(xcb)' 'pkgconfig(xcb-icccm)' \
		'pkgconfig(xcb-renderutil)' 'pkgconfig(xcb-errors)' 'pkgconfig(xcb-composite)' 'pkgconfig(xcb-res)' \
		xorg-x11-server-Xwayland 'pkgconfig(libseat)' 'pkgconfig(libdisplay-info)' 'pkgconfig(libliftoff)' \
		hwdata 'pkgconfig(gbm)' 'pkgconfig(egl)' 'pkgconfig(glesv2)' 'pkgconfig(vulkan)' glslang 'pkgconfig(lcms2)' \
		'pkgconfig(libudev)' 'pkgconfig(xwayland)' 'pkgconfig(libffi)' 'pkgconfig(expat)' \
		'cmake(Qt6Quick)' 'cmake(Qt6QuickControls2)' 'cmake(Qt6DBus)' 'cmake(Qt6WaylandClient)' 'cmake(LayerShellQt)' \
		qt6-qtdeclarative qt6-qtwayland qt6-qtsvg \
		dbus-daemon dbus-tools grim slurp wl-clipboard polkit
	try dnf install -y 'pkgconfig(wlroots-0.20)'
	for p in foot xdg-desktop-portal-wlr xdg-desktop-portal-gtk brightnessctl playerctl NetworkManager; do
		try dnf install -y $p
	done
	;;
debian)
	run apt-get update
	DEBIAN_FRONTEND=noninteractive; export DEBIAN_FRONTEND
	run env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
		build-essential git cmake meson ninja-build pkg-config ca-certificates \
		libwayland-dev wayland-protocols libinput-dev libxkbcommon-dev libpixman-1-dev libdrm-dev \
		libcairo2-dev libpango1.0-dev libpam0g-dev \
		libxcb1-dev libxcb-icccm4-dev libxcb-composite0-dev libxcb-res0-dev libxcb-render-util0-dev \
		libxcb-xfixes0-dev libxcb-ewmh-dev xwayland libffi-dev libexpat1-dev \
		libseat-dev libdisplay-info-dev libliftoff-dev hwdata libgbm-dev libegl-dev libgles-dev \
		libvulkan-dev glslang-tools liblcms2-dev libudev-dev \
		qt6-base-dev qt6-base-private-dev qt6-declarative-dev qt6-wayland-dev qt6-wayland-dev-tools \
		liblayershellqtinterface-dev layer-shell-qt \
		qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
		qml6-module-qtquick-window qml6-module-qtquick-templates qml6-module-qtqml-workerscript \
		qt6-wayland \
		dbus dbus-user-session grim slurp wl-clipboard polkitd pkexec
	try env DEBIAN_FRONTEND=noninteractive apt-get install -y libwlroots-0.20-dev
	for p in libxcb-errors-dev qt6-svg-plugins foot xdg-desktop-portal-wlr xdg-desktop-portal-gtk brightnessctl playerctl network-manager; do
		try env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends $p
	done
	;;
void)
	$SUDO xbps-install -Syu xbps >/dev/null 2>&1 || true
	run xbps-install -Sy base-devel git cmake meson ninja pkg-config \
		wayland-devel wayland-protocols libinput-devel libxkbcommon-devel pixman-devel libdrm-devel \
		cairo-devel pango-devel pam-devel libxcb-devel xcb-util-wm-devel xcb-util-renderutil-devel \
		xcb-util-errors-devel xorg-server-xwayland libseat-devel libdisplay-info-devel libliftoff-devel \
		MesaLib-devel libffi-devel expat-devel vulkan-loader-devel Vulkan-Headers glslang lcms2-devel eudev-libudev-devel \
		qt6-base-devel qt6-declarative-devel qt6-wayland-devel qt6-svg layer-shell-qt-devel \
		dbus elogind grim slurp wl-clipboard polkit
	try xbps-install -Sy wlroots0.20-devel
	for p in hwids hwdata foot xdg-desktop-portal-wlr xdg-desktop-portal-gtk brightnessctl playerctl NetworkManager; do
		try xbps-install -Sy $p
	done
	;;
suse)
	run zypper --non-interactive install gcc gcc-c++ git cmake meson ninja pkgconf \
		'pkgconfig(wayland-server)' 'pkgconfig(wayland-protocols)' 'pkgconfig(libinput)' \
		'pkgconfig(xkbcommon)' 'pkgconfig(pixman-1)' 'pkgconfig(libdrm)' 'pkgconfig(cairo)' \
		'pkgconfig(pangocairo)' pam-devel 'pkgconfig(xcb)' 'pkgconfig(xcb-icccm)' \
		'pkgconfig(xcb-renderutil)' 'pkgconfig(xcb-errors)' 'pkgconfig(xcb-composite)' 'pkgconfig(xcb-res)' \
		xwayland 'pkgconfig(libseat)' 'pkgconfig(libdisplay-info)' 'pkgconfig(libliftoff)' \
		hwdata 'pkgconfig(gbm)' 'pkgconfig(egl)' 'pkgconfig(glesv2)' 'pkgconfig(vulkan)' glslang-devel 'pkgconfig(lcms2)' \
		'pkgconfig(libudev)' \
		'cmake(Qt6Quick)' 'cmake(Qt6QuickControls2)' 'cmake(Qt6DBus)' 'cmake(Qt6WaylandClient)' 'cmake(LayerShellQt)' \
		qt6-declarative-imports qt6-wayland 'pkgconfig(libffi)' 'pkgconfig(expat)' \
		dbus-1 grim slurp wl-clipboard polkit
	try zypper --non-interactive install 'pkgconfig(wlroots-0.20)'
	for p in qt6-svg-imports libQt6Svg6 foot xdg-desktop-portal-wlr xdg-desktop-portal-gtk brightnessctl playerctl NetworkManager; do
		try zypper --non-interactive install $p
	done
	;;
alpine)
	run apk add build-base git cmake meson samurai pkgconf linux-headers \
		wayland-dev wayland-protocols libinput-dev libxkbcommon-dev pixman-dev libdrm-dev \
		cairo-dev pango-dev linux-pam-dev libxcb-dev xcb-util-wm-dev xcb-util-renderutil-dev \
		xcb-util-errors-dev xwayland xwayland-dev libseat-dev libdisplay-info-dev libliftoff-dev hwdata-dev \
		mesa-dev vulkan-loader-dev vulkan-headers glslang lcms2-dev eudev-dev libffi-dev expat-dev \
		qt6-qtbase-dev qt6-qtdeclarative-dev qt6-qtwayland-dev qt6-qtsvg layer-shell-qt-dev \
		dbus grim slurp wl-clipboard polkit
	try apk add wlroots0.20-dev
	for p in foot xdg-desktop-portal-wlr xdg-desktop-portal-gtk brightnessctl playerctl networkmanager; do
		try apk add $p
	done
	;;
gentoo)
	echo "Gentoo: emerge these, then run ./install.sh --no-deps:"
	echo "  dev-build/cmake dev-build/meson dev-build/ninja dev-libs/wayland dev-libs/wayland-protocols"
	echo "  dev-libs/libinput x11-libs/libxkbcommon x11-libs/pixman x11-libs/libdrm x11-libs/cairo x11-libs/pango"
	echo "  sys-libs/pam x11-libs/xcb-util-wm x11-base/xwayland sys-auth/seatd media-libs/libdisplay-info"
	echo "  dev-qt/qtbase:6 dev-qt/qtdeclarative:6 dev-qt/qtwayland:6 dev-qt/qtsvg:6 kde-plasma/layer-shell-qt"
	echo "  gui-apps/grim gui-apps/slurp gui-apps/wl-clipboard  (and gui-libs/wlroots:0.20 if available)"
	[ $PRINT -eq 1 ] || exit 2
	;;
nixos)
	echo "NixOS: build inside a nix-shell with wlroots_0_20, qt6.full, kdePackages.layer-shell-qt, cairo, pango, pam, libinput..."
	[ $PRINT -eq 1 ] || exit 2
	;;
*)
	echo "Unknown distro ($ID). Install the equivalents of: cmake meson ninja wayland(-protocols) libinput"
	echo "xkbcommon pixman libdrm cairo pango pam xcb-util-wm xwayland seatd libdisplay-info"
	echo "Qt 6 (base, declarative, wayland, svg) layer-shell-qt grim slurp wl-clipboard — then ./install.sh --no-deps"
	[ $PRINT -eq 1 ] || exit 2
	;;
esac
