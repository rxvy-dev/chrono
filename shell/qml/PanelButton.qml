// A themed clickable area: icon and/or text on a Skin.
import QtQuick

Item {
	id: b
	property alias hovered: ma.containsMouse
	property bool checked: false
	property bool attention: false
	property bool underline: false
	property color fgOverride: "transparent"   // set to force a text colour (e.g. on a light menu)
	property string text: ""
	property string glyph: ""
	property string icon: ""            // freedesktop icon name
	property int iconSize: 16
	property bool bold: false
	property string kind: "button"
	property alias skin: sk
	property int padding: 6
	property real maxWidth: 99999
	readonly property alias pressed: ma.pressed
	readonly property color fg: fgOverride.a > 0 ? fgOverride : sk.fg
	signal clicked(var mouse)
	signal rightClicked(var mouse)
	signal middleClicked(var mouse)
	signal wheel(var wheel)
	default property alias content: extra.data

	implicitHeight: 24
	implicitWidth: Math.min(maxWidth, row.implicitWidth + padding * 2)

	Skin { id: sk; anchors.fill: parent; kind: b.kind; hovered: ma.containsMouse; pressed: ma.pressed; checked: b.checked; attention: b.attention; underline: b.underline }

	Row {
		id: row
		anchors.verticalCenter: parent.verticalCenter
		x: b.padding + (sk.sunken && sk.bevel ? 1 : 0)
		y: sk.sunken && sk.bevel ? 1 : 0
		width: b.width - b.padding * 2
		spacing: 6
		clip: true
		Image {
			visible: b.icon !== ""
			source: b.icon !== "" ? "image://icon/" + encodeURIComponent(b.icon) : ""
			sourceSize: Qt.size(b.iconSize, b.iconSize)
			width: b.iconSize; height: b.iconSize
			anchors.verticalCenter: parent.verticalCenter
			smooth: true
			asynchronous: true
		}
		Glyph {
			visible: b.glyph !== ""
			name: b.glyph
			color: b.fg
			width: b.iconSize; height: b.iconSize
			anchors.verticalCenter: parent.verticalCenter
		}
		Text {
			visible: b.text !== ""
			text: b.text
			color: b.fg
			font.family: Chrono.theme.fontUi.family
			font.pointSize: Chrono.theme.fontUi.size
			font.bold: b.bold
			elide: Text.ElideRight
			width: Math.min(implicitWidth, Math.max(0, b.maxWidth - b.padding * 2 - (b.icon !== "" || b.glyph !== "" ? b.iconSize + 6 : 0)))
			anchors.verticalCenter: parent.verticalCenter
		}
		Row { id: extra; spacing: 6; height: b.height }
	}

	MouseArea {
		id: ma
		anchors.fill: parent
		hoverEnabled: true
		acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
		onClicked: (m) => {
			if (m.button === Qt.RightButton) b.rightClicked(m)
			else if (m.button === Qt.MiddleButton) b.middleClicked(m)
			else b.clicked(m)
		}
		onWheel: (w) => b.wheel(w)
	}
}
