// The look of every surface and button, chosen from the theme: 3D bevels for 90s themes,
// hard 1-px frames for pixel themes, soft rounded translucency for modern ones.
import QtQuick
import QtQuick.Window

Item {
	id: skin
	property string kind: "button"        // "panel" | "popup" | "button" | "field" | "well"
	property bool hovered: false
	property bool pressed: false
	property bool checked: false
	property bool attention: false
	property bool underline: false      // accent bar under checked taskbar buttons
	readonly property var t: Chrono.theme
	readonly property string style: t.panelStyle || "flat"
	readonly property bool bevel: style === "bevel"
	readonly property bool pixel: style === "pixel"
	readonly property bool soft: !bevel && !pixel
	readonly property real radius: kind === "panel" ? (style === "floating" ? Math.min(14, height / 2) : 0)
				     : kind === "popup" ? (soft ? Math.max(6, Math.min(16, t.cornerRadius + 4)) : 0)
				     : soft ? Math.max(4, Math.min(10, t.cornerRadius)) : 0
	readonly property color fg: pixel && kind === "button" && (pressed || checked) ? t.panelBg
				 : (bevel && kind !== "panel") || (pixel && kind !== "panel") ? (kind === "field" ? t.windowFg : t.buttonFg) : t.panelFg
	readonly property bool sunken: (kind === "button" && (pressed || checked)) || kind === "field" || kind === "well"

	function mix(a, b, f) { return Qt.rgba(a.r * (1 - f) + b.r * f, a.g * (1 - f) + b.g * f, a.b * (1 - f) + b.b * f, a.a * (1 - f) + b.a * f) }

	// ---------- frosted backdrop for glass popups: the wallpaper, blurred, cut to this rectangle.
	// (wlroots can't blur what's behind a surface, so we blur the wallpaper we painted ourselves)
	readonly property var win: skin.Window.window
	readonly property bool frosted: style === "glass" && kind === "popup" && !!win && win.isPopupLayer === true && Chrono.motion >= 0
	property int frostGen: 0
	Image {
		id: frost
		anchors.fill: parent
		visible: skin.frosted && status === Image.Ready
		z: -1
		cache: false
		asynchronous: true
		function url() {
			if (!skin.frosted || !skin.win || !skin.win.screen || skin.width < 2) return ""
			const p = skin.mapToItem(null, 0, 0)
			const topBar = Chrono.panelPosition === "top" ? (t.panelHeight + (t.panelStyle === "floating" && !Chrono.dockEnabled ? 8 : 0)) : 0
			return "image://glass/" + encodeURIComponent(Chrono.selectedTheme + "?sw=" + skin.win.screen.width + "&sh=" + skin.win.screen.height
				+ "&x=" + Math.round(p.x) + "&y=" + Math.round(p.y + topBar) + "&w=" + Math.round(skin.width) + "&h=" + Math.round(skin.height)
				+ "&r=" + Math.round(skin.radius)) + "#" + skin.frostGen
		}
		source: ""
		Timer { id: refrost; interval: 30; onTriggered: frost.source = frost.url() }
	}
	onFrostedChanged: refrost.restart()
	onWidthChanged: if (frosted) refrost.restart()
	onHeightChanged: if (frosted) refrost.restart()
	onVisibleChanged: if (frosted && visible) refrost.restart()
	Connections { target: Chrono; enabled: skin.frosted; function onThemeChanged() { skin.frostGen++; refrost.restart() } }
	Connections { target: skin.win; enabled: skin.frosted; ignoreUnknownSignals: true
		function onVisibleChanged() { if (skin.win.visible) refrost.restart() }
		function onTargetXChanged() { refrost.restart() } function onTargetYChanged() { refrost.restart() } }

	// ---------- soft (flat / floating / glass)
	Shadow {
		anchors.fill: parent
		visible: skin.soft && (skin.kind === "popup" || (skin.kind === "panel" && skin.style === "floating"))
		radius: skin.radius
		size: skin.kind === "popup" ? 22 : 10
		strength: skin.kind === "popup" ? 0.55 : 0.3
		offsetY: skin.kind === "popup" ? 8 : 3
		z: -1
	}
	Rectangle {
		anchors.fill: parent
		visible: skin.soft
		radius: skin.radius
		color: {
			if (skin.kind === "panel") return t.panelBg
			if (skin.kind === "popup" && skin.style === "glass")   // a tint over the frosted wallpaper (or solid-ish without it)
				return t.panelFg.hslLightness > 0.5 ? Qt.rgba(0.08, 0.08, 0.12, frost.visible ? 0.55 : 0.94)
								    : Qt.rgba(1, 1, 1, frost.visible ? 0.6 : 0.95)
			if (skin.kind === "popup") return Qt.rgba(t.panelBg.r, t.panelBg.g, t.panelBg.b, Math.max(t.panelBg.a, 0.97))
			if (skin.kind === "field" || skin.kind === "well") return Qt.rgba(t.panelFg.r, t.panelFg.g, t.panelFg.b, 0.08)
			if (skin.pressed) return Qt.rgba(t.panelFg.r, t.panelFg.g, t.panelFg.b, 0.22)
			if (skin.checked) return Qt.rgba(t.accent.r, t.accent.g, t.accent.b, 0.35)
			if (skin.hovered) return Qt.rgba(t.panelFg.r, t.panelFg.g, t.panelFg.b, 0.12)
			if (skin.attention) return Qt.rgba(t.accent.r, t.accent.g, t.accent.b, 0.25)
			return "transparent"
		}
		border.width: (skin.kind === "panel" || skin.kind === "popup") ? 1 : 0
		border.color: t.panelBorder
		Behavior on color { ColorAnimation { duration: 90 } }
		// gloss (2000s taskbars): a bright top half and a darker bottom edge
		Rectangle {
			visible: skin.style === "gloss" && (skin.kind === "panel" || (skin.kind === "button" && (skin.checked || skin.hovered || skin.pressed)))
			anchors.fill: parent
			radius: skin.radius
			gradient: Gradient {
				GradientStop { position: 0.0; color: Qt.rgba(1, 1, 1, skin.pressed ? 0.05 : 0.32) }
				GradientStop { position: 0.45; color: Qt.rgba(1, 1, 1, 0.06) }
				GradientStop { position: 0.55; color: Qt.rgba(0, 0, 0, 0.0) }
				GradientStop { position: 1.0; color: Qt.rgba(0, 0, 0, skin.pressed ? 0.25 : 0.18) }
			}
		}
		// glass: a lit top edge
		Rectangle {
			visible: skin.style === "glass" && (skin.kind === "panel" || skin.kind === "popup")
			anchors { left: parent.left; right: parent.right; top: parent.top; margins: 1 }
			height: Math.min(parent.height / 2, 18)
			radius: skin.radius
			gradient: Gradient {
				GradientStop { position: 0; color: Qt.rgba(1, 1, 1, 0.22) }
				GradientStop { position: 1; color: Qt.rgba(1, 1, 1, 0) }
			}
		}
		// checked underline for taskbar buttons
		Rectangle {
			visible: skin.kind === "button" && skin.checked && skin.underline
			anchors { horizontalCenter: parent.horizontalCenter; bottom: parent.bottom; bottomMargin: 2 }
			width: Math.min(parent.width - 8, 18); height: 2; radius: 1
			color: t.accent
		}
	}

	// ---------- bevel (90s)
	Item {
		anchors.fill: parent
		visible: skin.bevel
		// classic 3D UIs paint menus and buttons in the window colour (button_bg is the title-button colour)
		readonly property color face: skin.kind === "field" ? t.windowBg : (skin.kind === "panel" ? t.panelBg : t.bg)
		readonly property color light: skin.sunken ? t.bevelShadow : t.bevelLight
		readonly property color dark: skin.sunken ? t.bevelLight : t.bevelShadow
		readonly property color mid: skin.sunken ? t.bevelDark : t.bevelDark
		Rectangle { anchors.fill: parent; color: skin.kind === "button" && skin.checked ? skin.mix(parent.face, t.bevelLight, 0.5) : parent.face
			visible: skin.kind !== "button" || skin.checked || skin.pressed || skin.hovered || skin.kind === "button" }
		// panels: only a light line on the edge facing the desktop
		Rectangle { visible: skin.kind === "panel"; color: t.panelBorder; height: 1
			anchors { left: parent.left; right: parent.right }
			y: Chrono.panelPosition === "top" ? parent.height - 1 : 0 }
		Item {
			anchors.fill: parent
			visible: skin.kind !== "panel"
			Rectangle { x: 0; y: 0; width: parent.width; height: 1; color: skin.sunken ? t.bevelDark : t.bevelLight }
			Rectangle { x: 0; y: 0; width: 1; height: parent.height; color: skin.sunken ? t.bevelDark : t.bevelLight }
			Rectangle { x: 0; y: parent.height - 1; width: parent.width; height: 1; color: skin.sunken ? t.bevelLight : t.bevelShadow }
			Rectangle { x: parent.width - 1; y: 0; width: 1; height: parent.height; color: skin.sunken ? t.bevelLight : t.bevelShadow }
			Rectangle { x: 1; y: parent.height - 2; width: parent.width - 2; height: 1; color: skin.sunken ? "transparent" : t.bevelDark; visible: skin.kind !== "well" }
			Rectangle { x: parent.width - 2; y: 1; width: 1; height: parent.height - 2; color: skin.sunken ? "transparent" : t.bevelDark; visible: skin.kind !== "well" }
			Rectangle { x: 1; y: 1; width: parent.width - 2; height: 1; color: skin.sunken && skin.kind !== "well" ? t.bevelShadow : "transparent" }
			Rectangle { x: 1; y: 1; width: 1; height: parent.height - 2; color: skin.sunken && skin.kind !== "well" ? t.bevelShadow : "transparent" }
		}
	}

	// ---------- pixel (1-bit era)
	Rectangle {
		anchors.fill: parent
		visible: skin.pixel
		color: skin.kind === "button" && (skin.pressed || skin.checked) ? t.panelFg
		     : skin.kind === "panel" ? t.panelBg : skin.kind === "button" ? t.buttonBg : t.windowBg
		border.width: skin.kind === "panel" ? 0 : 1
		border.color: t.panelBorder
		Rectangle { visible: skin.kind === "panel"; color: t.panelBorder; height: 2
			anchors { left: parent.left; right: parent.right }
			y: Chrono.panelPosition === "top" ? parent.height - 2 : 0 }
		Rectangle { visible: skin.kind === "popup"; x: 3; y: parent.height; width: parent.width; height: 3; color: t.panelBorder }
		Rectangle { visible: skin.kind === "popup"; x: parent.width; y: 3; width: 3; height: parent.height; color: t.panelBorder }
	}
}
