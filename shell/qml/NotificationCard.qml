import QtQuick

Item {
	id: card
	property var note: ({})
	property bool inHistory: false
	readonly property var t: Chrono.theme
	height: Math.max(56, content.implicitHeight + 20)

	Skin { anchors.fill: parent; kind: card.inHistory ? "well" : "popup"; attention: note.urgency >= 2 }
	Rectangle { visible: note.urgency >= 2; width: 3; height: parent.height - 8; x: 4; y: 4; color: "#e5484d"; radius: 1 }

	Image {
		id: ic
		x: 10; y: 10
		width: 32; height: 32
		source: note.icon ? (note.icon.indexOf("/") === 0 ? "file://" + note.icon : "image://icon/" + encodeURIComponent(note.icon)) : ""
		sourceSize: Qt.size(64, 64)
		fillMode: Image.PreserveAspectFit
		visible: source != "" && status === Image.Ready
		asynchronous: true
	}
	Column {
		id: content
		anchors { left: ic.visible ? ic.right : parent.left; leftMargin: 10; right: parent.right; rightMargin: 28; top: parent.top; topMargin: 10 }
		spacing: 3
		Text { width: parent.width; text: note.app + (card.inHistory && note.time ? "  ·  " + Qt.formatTime(note.time, Qt.locale().timeFormat(Locale.ShortFormat)) : "")
			color: t.panelFg; opacity: 0.6; elide: Text.ElideRight; font.family: t.fontUi.family; font.pointSize: t.fontUi.size - 1 }
		Text { width: parent.width; text: note.summary || ""; color: t.panelFg; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight
			font.family: t.fontUi.family; font.pointSize: t.fontUi.size; font.bold: true }
		Text { width: parent.width; text: note.body || ""; visible: text !== ""; color: t.panelFg; wrapMode: Text.Wrap; maximumLineCount: card.inHistory ? 3 : 5
			elide: Text.ElideRight; textFormat: Text.StyledText; linkColor: t.accent; onLinkActivated: (l) => Qt.openUrlExternally(l)
			font.family: t.fontUi.family; font.pointSize: t.fontUi.size }
		Row {
			spacing: 6
			visible: (note.actions || []).length > 0
			Repeater {
				model: (note.actions || []).filter(a => a.id !== "default")
				PanelButton { required property var modelData; height: 26; text: modelData.label
					onClicked: Notifications.invoke(card.note.id, modelData.id) }
			}
		}
	}
	MouseArea {
		anchors.fill: parent
		z: -1
		onClicked: {
			if ((card.note.actions || []).some(a => a.id === "default")) Notifications.invoke(card.note.id, "default")
			else card.inHistory ? Notifications.remove(card.note.id) : Notifications.dismiss(card.note.id)
		}
	}
	PanelButton {
		anchors { right: parent.right; top: parent.top; margins: 4 }
		width: 22; height: 22; padding: 3; glyph: "close"; iconSize: 12
		onClicked: card.inHistory ? Notifications.remove(card.note.id) : Notifications.dismiss(card.note.id)
	}
}
