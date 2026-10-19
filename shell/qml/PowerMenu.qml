// Shut down / restart / sleep / log out / lock, centred over a dimmed screen.
import QtQuick

PopupLayer {
	id: pm
	readonly property var t: Chrono.theme
	layerName: "overlay"
	keyboard: "exclusive"
	center: true
	contentWidth: row.implicitWidth + 40
	contentHeight: 186
	property int current: 0
	readonly property var entries: [
		{ glyph: "power", text: qsTr("Shut down"), run: () => System.power("shutdown") },
		{ glyph: "reboot", text: qsTr("Restart"), run: () => System.power("reboot") },
		{ glyph: "sleep", text: qsTr("Sleep"), run: () => { Chrono.command("lock"); System.power("suspend") } },
		{ glyph: "logout", text: qsTr("Log out"), run: () => Chrono.command("quit") },
		{ glyph: "lock", text: qsTr("Lock"), run: () => Chrono.command("lock") },
	]
	onOpened: { current = 4; anim.restart() }   // Lock is the safe default for Enter

	Rectangle {
		parent: pm.contentItem
		anchors.fill: parent
		z: -1
		color: Qt.rgba(0, 0, 0, 0.45)
	}
	ParallelAnimation {
		id: anim
		NumberAnimation { target: pm.box; property: "opacity"; from: Chrono.motion ? 0 : 1; to: 1; duration: 180 * Chrono.motion }
		NumberAnimation { target: pm.box; property: "scale"; from: Chrono.motion ? 0.95 : 1; to: 1; duration: 200 * Chrono.motion; easing.type: Easing.OutCubic }
	}
	Skin { anchors.fill: parent; kind: "popup" }
	Text { id: head; anchors.horizontalCenter: parent.horizontalCenter; y: 20; text: qsTr("Goodbye, %1").arg(System.userName)
		color: t.panelFg; font.family: t.fontUi.family; font.pointSize: t.fontUi.size + 2; font.bold: true }
	Row {
		id: row
		anchors.horizontalCenter: parent.horizontalCenter
		anchors.top: head.bottom; anchors.topMargin: 16
		spacing: 10
		Repeater {
			model: pm.entries
			Item {
				required property var modelData
				required property int index
				width: 92; height: 96
				Skin { anchors.fill: parent; kind: "button"; hovered: ma.containsMouse; pressed: ma.pressed; checked: pm.current === index }
				Column {
					anchors.centerIn: parent
					spacing: 8
					Glyph { anchors.horizontalCenter: parent.horizontalCenter; name: modelData.glyph; width: 32; height: 32; color: Chrono.theme.panelFg }
					Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.text; color: Chrono.theme.panelFg
						font.family: Chrono.theme.fontUi.family; font.pointSize: Chrono.theme.fontUi.size }
				}
				MouseArea { id: ma; anchors.fill: parent; hoverEnabled: true; onEntered: pm.current = index; onClicked: { pm.close(); modelData.run() } }
			}
		}
	}
	Item {
		focus: true
		Keys.onLeftPressed: pm.current = (pm.current + pm.entries.length - 1) % pm.entries.length
		Keys.onRightPressed: pm.current = (pm.current + 1) % pm.entries.length
		Keys.onReturnPressed: { pm.close(); pm.entries[pm.current].run() }
		Keys.onEscapePressed: pm.close()
	}
}
