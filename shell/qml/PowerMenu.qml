// Shut down / restart / sleep / log out / lock, centred over a dimmed screen.
import QtQuick

PopupLayer {
	id: P
	readonly property var t: Chrono.theme
	layerName: "overlay"
	keyboard: "exclusive"
	center: true
	contentWidth: row.implicitWidth + 40
	contentHeight: 170
	property int current: 0
	readonly property var entries: [
		{ glyph: "power", text: qsTr("Shut down"), run: () => System.power("shutdown") },
		{ glyph: "reboot", text: qsTr("Restart"), run: () => System.power("reboot") },
		{ glyph: "sleep", text: qsTr("Sleep"), run: () => { Chrono.command("lock"); System.power("suspend") } },
		{ glyph: "logout", text: qsTr("Log out"), run: () => Chrono.command("quit") },
		{ glyph: "lock", text: qsTr("Lock"), run: () => Chrono.command("lock") },
	]
	onOpened: current = 0

	Rectangle {
		parent: P.contentItem
		anchors.fill: parent
		z: -1
		color: Qt.rgba(0, 0, 0, 0.45)
	}
	Skin { anchors.fill: parent; kind: "popup" }
	Text { id: head; anchors.horizontalCenter: parent.horizontalCenter; y: 14; text: qsTr("Goodbye, %1").arg(System.userName)
		color: t.panelFg; font.family: t.fontUi.family; font.pointSize: t.fontUi.size + 2; font.bold: true }
	Row {
		id: row
		anchors.horizontalCenter: parent.horizontalCenter
		anchors.top: head.bottom; anchors.topMargin: 16
		spacing: 10
		Repeater {
			model: P.entries
			Item {
				required property var modelData
				required property int index
				width: 92; height: 96
				Skin { anchors.fill: parent; kind: "button"; hovered: ma.containsMouse; pressed: ma.pressed; checked: P.current === index }
				Column {
					anchors.centerIn: parent
					spacing: 8
					Glyph { anchors.horizontalCenter: parent.horizontalCenter; name: modelData.glyph; width: 32; height: 32; color: Chrono.theme.panelFg }
					Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.text; color: Chrono.theme.panelFg
						font.family: Chrono.theme.fontUi.family; font.pointSize: Chrono.theme.fontUi.size }
				}
				MouseArea { id: ma; anchors.fill: parent; hoverEnabled: true; onEntered: P.current = index; onClicked: { P.close(); modelData.run() } }
			}
		}
	}
	Item {
		focus: true
		Keys.onLeftPressed: P.current = (P.current + P.entries.length - 1) % P.entries.length
		Keys.onRightPressed: P.current = (P.current + 1) % P.entries.length
		Keys.onReturnPressed: { P.close(); P.entries[P.current].run() }
		Keys.onEscapePressed: P.close()
	}
}
