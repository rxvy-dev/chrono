// Clock popup: a month calendar and the notification history.
import QtQuick

PopupLayer {
	id: C
	readonly property var t: Chrono.theme
	contentWidth: 600
	contentHeight: 330
	property date shown: new Date()
	onOpened: shown = new Date()

	Skin { anchors.fill: parent; kind: "popup" }

	// ----- notifications
	Item {
		id: notes
		anchors { left: parent.left; top: parent.top; bottom: parent.bottom; margins: 12 }
		width: 290
		Text { id: nh; text: qsTr("Notifications"); color: t.panelFg; font.family: t.fontUi.family; font.pointSize: t.fontUi.size + 1; font.bold: true }
		PanelButton { anchors.right: parent.right; anchors.verticalCenter: nh.verticalCenter; height: 24; visible: Notifications.history.length > 0
			text: qsTr("Clear"); onClicked: Notifications.clearHistory() }
		ListView {
			anchors { top: nh.bottom; topMargin: 10; left: parent.left; right: parent.right; bottom: parent.bottom }
			clip: true
			spacing: 6
			model: Notifications.history
			delegate: NotificationCard { required property var modelData; note: modelData; width: notes.width; inHistory: true }
			Text { anchors.centerIn: parent; visible: Notifications.history.length === 0; text: qsTr("No notifications")
				color: t.panelFg; opacity: 0.5; font.family: t.fontUi.family; font.pointSize: t.fontUi.size }
		}
	}
	Rectangle { x: notes.x + notes.width + 10; y: 12; width: 1; height: parent.height - 24; color: t.panelBorder }

	// ----- calendar
	Column {
		anchors { right: parent.right; top: parent.top; margins: 12 }
		width: 260
		spacing: 6
		Text { text: Qt.formatDate(new Date(), "dddd"); color: t.panelFg; opacity: 0.7; font.family: t.fontUi.family; font.pointSize: t.fontUi.size }
		Text { text: Qt.formatDate(new Date(), "d MMMM yyyy"); color: t.panelFg; font.family: t.fontUi.family; font.pointSize: t.fontUi.size + 4; font.bold: true }
		Item {
			width: parent.width; height: 26
			PanelButton { glyph: "prev"; height: 24; iconSize: 10; onClicked: C.shown = new Date(C.shown.getFullYear(), C.shown.getMonth() - 1, 1) }
			Text { anchors.centerIn: parent; text: Qt.formatDate(C.shown, "MMMM yyyy"); color: t.panelFg; font.family: t.fontUi.family; font.pointSize: t.fontUi.size; font.bold: true }
			PanelButton { anchors.right: parent.right; glyph: "next"; height: 24; iconSize: 10; onClicked: C.shown = new Date(C.shown.getFullYear(), C.shown.getMonth() + 1, 1) }
		}
		Grid {
			id: grid
			columns: 7
			readonly property int first: Qt.locale().firstDayOfWeek
			readonly property var start: { const d = new Date(C.shown.getFullYear(), C.shown.getMonth(), 1); const off = (d.getDay() - first + 7) % 7; return new Date(d.getFullYear(), d.getMonth(), 1 - off) }
			Repeater {
				model: 7
				Text { required property int index; width: 37; height: 22; horizontalAlignment: Text.AlignHCenter
					text: Qt.locale().dayName((grid.first + index) % 7, Locale.NarrowFormat)
					color: t.panelFg; opacity: 0.6; font.family: t.fontUi.family; font.pointSize: t.fontUi.size - 1 }
			}
			Repeater {
				model: 42
				Item {
					required property int index
					readonly property date day: new Date(grid.start.getFullYear(), grid.start.getMonth(), grid.start.getDate() + index)
					readonly property bool today: day.toDateString() === new Date().toDateString()
					width: 37; height: 28
					Rectangle { anchors.centerIn: parent; width: 26; height: 26; radius: t.panelStyle === "bevel" || t.panelStyle === "pixel" ? 0 : 13
						color: parent.today ? t.accent : "transparent" }
					Text { anchors.centerIn: parent; text: parent.day.getDate()
						color: parent.today ? t.accentFg : t.panelFg
						opacity: parent.day.getMonth() === C.shown.getMonth() ? 1 : 0.35
						font.family: t.fontUi.family; font.pointSize: t.fontUi.size }
				}
			}
		}
	}
}
