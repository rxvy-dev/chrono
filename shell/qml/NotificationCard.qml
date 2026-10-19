// One notification: app icon, app name and time, title, body, action buttons; popups also show a
// bar that runs down until the notification goes away.
import QtQuick

Item {
	id: card
	property var note: ({})
	property bool popup: false
	readonly property bool inHistory: !popup
	readonly property var t: Chrono.theme
	readonly property bool retro: t.panelStyle === "bevel" || t.panelStyle === "pixel"
	readonly property color fg: retro ? t.windowFg : t.panelFg
	height: Math.max(64, content.implicitHeight + 28)

	Skin { anchors.fill: parent; kind: card.popup ? "popup" : "well" }
	Rectangle { visible: note.urgency >= 2; width: 3; height: parent.height - 16; x: 6; y: 8; color: "#e5484d"; radius: 2 }

	Item {
		id: iconBox
		x: 14; y: 14
		width: 36; height: 36
		Image {
			id: ic
			anchors.fill: parent
			source: note.icon ? (note.icon.indexOf("/") === 0 ? "file://" + note.icon : "image://icon/" + encodeURIComponent(note.icon)) : ""
			sourceSize: Qt.size(72, 72)
			fillMode: Image.PreserveAspectFit
			visible: source != "" && status === Image.Ready
			asynchronous: true
		}
		Rectangle {   // fallback: the app's initial on an accent disc
			anchors.fill: parent
			visible: !ic.visible
			radius: card.retro ? 0 : width / 2
			color: Qt.rgba(t.accent.r, t.accent.g, t.accent.b, 0.85)
			Text { anchors.centerIn: parent; text: (card.note.app || "?").charAt(0).toUpperCase(); color: t.accentFg
				font.family: t.fontUi.family; font.pointSize: t.fontUi.size + 3; font.bold: true }
		}
	}
	Column {
		id: content
		anchors { left: iconBox.right; leftMargin: 12; right: parent.right; rightMargin: 32; top: parent.top; topMargin: 12 }
		spacing: 3
		Text { width: parent.width; text: (note.app || "") + (note.time ? "  ·  " + Qt.formatTime(new Date(note.time), Qt.locale().timeFormat(Locale.ShortFormat).replace(":ss", "")) : "")
			color: card.fg; opacity: 0.6; elide: Text.ElideRight; font.family: t.fontUi.family; font.pointSize: t.fontUi.size - 1 }
		Text { width: parent.width; text: note.summary || ""; color: card.fg; wrapMode: Text.Wrap; maximumLineCount: 2; elide: Text.ElideRight
			font.family: t.fontUi.family; font.pointSize: t.fontUi.size + 0.5; font.weight: Font.DemiBold }
		Text { width: parent.width; text: note.body || ""; visible: text !== ""; color: card.fg; opacity: 0.85; wrapMode: Text.Wrap
			maximumLineCount: card.inHistory ? 3 : 5; elide: Text.ElideRight; textFormat: Text.StyledText; linkColor: t.accent
			onLinkActivated: (l) => Qt.openUrlExternally(l)
			font.family: t.fontUi.family; font.pointSize: t.fontUi.size }
		Row {
			spacing: 6
			topPadding: 6
			visible: (note.actions || []).filter(a => a.id !== "default").length > 0
			Repeater {
				model: (note.actions || []).filter(a => a.id !== "default")
				PanelButton { required property var modelData; height: 28; padding: 12; text: modelData.label; kind: "button"
					Component.onCompleted: skin.attention = true
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
		anchors { right: parent.right; top: parent.top; margins: 6 }
		width: 24; height: 24; padding: 6; glyph: "close"; iconSize: 12
		onClicked: card.inHistory ? Notifications.remove(card.note.id) : Notifications.dismiss(card.note.id)
	}
	// time left
	Rectangle {
		id: bar
		visible: card.popup && note.timeout > 0
		anchors { left: parent.left; bottom: parent.bottom; leftMargin: card.retro ? 3 : 10; bottomMargin: card.retro ? 3 : 6 }
		height: 3; radius: 1.5
		color: t.accent
		opacity: 0.8
		width: card.width - (card.retro ? 6 : 20)
		NumberAnimation on width {
			running: card.popup && card.note.timeout > 0
			from: card.width - (card.retro ? 6 : 20); to: 0
			duration: card.note.timeout || 1
		}
	}
}
