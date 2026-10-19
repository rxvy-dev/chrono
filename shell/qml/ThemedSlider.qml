import QtQuick

Item {
	id: sl
	property real value: 0.5     // 0..1
	property string glyph: ""
	property bool glyphFlag: false
	signal moved(real value)
	signal glyphClicked()
	readonly property var t: Chrono.theme
	readonly property bool bevel: t.panelStyle === "bevel"
	readonly property bool pixel: t.panelStyle === "pixel"
	implicitHeight: 28
	implicitWidth: 200

	PanelButton {
		id: gb
		visible: sl.glyph !== ""
		width: 28; height: 28
		anchors.verticalCenter: parent.verticalCenter
		onClicked: sl.glyphClicked()
	}
	Glyph { visible: sl.glyph !== ""; anchors.centerIn: gb; name: sl.glyph; flag: sl.glyphFlag; value: sl.value; color: Chrono.theme.panelFg; width: 16; height: 16 }

	Item {
		id: track
		anchors { left: sl.glyph !== "" ? gb.right : parent.left; leftMargin: 6; right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
		height: 20
		Skin { kind: "well"; anchors.verticalCenter: parent.verticalCenter; width: parent.width; height: sl.bevel ? 6 : (sl.pixel ? 6 : 4); visible: sl.bevel || sl.pixel }
		Rectangle {
			visible: !sl.bevel && !sl.pixel
			anchors.verticalCenter: parent.verticalCenter
			width: parent.width; height: 4; radius: 2
			color: Qt.rgba(t.panelFg.r, t.panelFg.g, t.panelFg.b, 0.2)
			Rectangle { width: parent.width * Math.max(0, Math.min(1, sl.value)); height: parent.height; radius: 2; color: t.accent }
		}
		Item {
			id: knob
			width: sl.bevel ? 11 : 16; height: sl.bevel ? 20 : 16
			anchors.verticalCenter: parent.verticalCenter
			x: (track.width - width) * Math.max(0, Math.min(1, sl.value))
			Skin { anchors.fill: parent; kind: "button"; visible: sl.bevel || sl.pixel }
			Rectangle { anchors.fill: parent; radius: width / 2; visible: !sl.bevel && !sl.pixel; color: "white"; border.color: Qt.rgba(0, 0, 0, 0.2) }
		}
		MouseArea {
			anchors.fill: parent
			anchors.margins: -6
			function set(mx) { const v = Math.max(0, Math.min(1, (mx - 6) / track.width)); sl.value = v; sl.moved(v) }
			onPressed: (m) => set(m.x)
			onPositionChanged: (m) => { if (pressed) set(m.x) }
			onWheel: (w) => { const v = Math.max(0, Math.min(1, sl.value + (w.angleDelta.y > 0 ? 0.05 : -0.05))); sl.value = v; sl.moved(v) }
		}
	}
}
