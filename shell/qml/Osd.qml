// On-screen display for volume / brightness / mic keys.
import QtQuick
import QtQuick.Window

Window {
	id: osd
	property string kind: "volume"
	readonly property var t: Chrono.theme
	color: "transparent"
	flags: Qt.FramelessWindowHint
	width: 300; height: 64
	visible: false

	function show(k) {
		kind = k
		if (!visible) {
			Layer.setup(osd, { screen: osd.screen, layer: "overlay", anchors: 2, exclusive: -1, keyboard: "none", scope: "chrono-osd", margins: [0, 0, 120, 0] })
			visible = true
		}
		hide.restart()
	}
	Timer { id: hide; interval: 1600; onTriggered: osd.visible = false }

	readonly property real level: kind === "brightness" ? System.brightness / 100 : kind === "mic" ? (System.micMuted ? 0 : 1) : System.volume / 100
	Skin { anchors.fill: parent; kind: "popup" }
	Glyph { id: g; x: 18; anchors.verticalCenter: parent.verticalCenter; width: 24; height: 24; color: t.panelFg
		name: osd.kind; value: osd.level; flag: osd.kind === "mic" ? System.micMuted : osd.kind === "volume" && System.muted }
	Item {
		anchors { left: g.right; leftMargin: 16; right: pct.left; rightMargin: 12; verticalCenter: parent.verticalCenter }
		height: 8
		Skin { anchors.fill: parent; kind: "well"; visible: t.panelStyle === "bevel" || t.panelStyle === "pixel" }
		Rectangle { anchors.fill: parent; radius: 4; visible: t.panelStyle !== "bevel" && t.panelStyle !== "pixel"; color: Qt.rgba(t.panelFg.r, t.panelFg.g, t.panelFg.b, 0.2) }
		Rectangle { x: t.panelStyle === "bevel" ? 2 : 0; y: x; height: parent.height - 2 * x; radius: t.panelStyle === "bevel" || t.panelStyle === "pixel" ? 0 : 4
			width: (parent.width - 2 * x) * Math.min(1, osd.level); color: t.accent
			Behavior on width { NumberAnimation { duration: 80 } } }
	}
	Text { id: pct; anchors.right: parent.right; anchors.rightMargin: 16; anchors.verticalCenter: parent.verticalCenter
		text: osd.kind === "mic" ? (System.micMuted ? qsTr("Off") : qsTr("On")) : Math.round(osd.level * 100)
		width: 30; horizontalAlignment: Text.AlignRight
		color: t.panelFg; font.family: t.fontUi.family; font.pointSize: t.fontUi.size + 1; font.bold: true }
}
