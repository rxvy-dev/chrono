// A simple themed context menu. items: [{text, glyph, checked, separator, run: function}]
import QtQuick

PopupLayer {
	id: M
	property var items: []
	property string title: ""
	readonly property var t: Chrono.theme
	contentWidth: 220
	contentHeight: col.implicitHeight + 8

	function show(scr, x, list, heading, y) {
		items = list
		title = heading || ""
		open(scr, x + (y === undefined ? 0 : contentWidth / 2), y)
	}

	Skin { anchors.fill: parent; kind: "popup" }
	Column {
		id: col
		x: 4; y: 4
		width: parent.width - 8
		Text { visible: M.title !== ""; width: parent.width; text: M.title; elide: Text.ElideRight; padding: 6
			color: t.panelFg; opacity: 0.65; font.family: t.fontUi.family; font.pointSize: t.fontUi.size - 1; font.bold: true }
		Repeater {
			model: M.items
			Loader {
				required property var modelData
				width: col.width
				sourceComponent: modelData.separator ? sep : entry
				Component { id: sep; Item { height: 7; Rectangle { y: 3; width: parent.width; height: 1; color: t.panelBorder } } }
				Component {
					id: entry
					PanelButton {
						height: 28
						glyph: modelData.glyph || ""
						text: modelData.text
						checked: !!modelData.checked
						onClicked: { M.close(); if (modelData.run) modelData.run() }
					}
				}
			}
		}
	}
}
