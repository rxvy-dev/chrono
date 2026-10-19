import QtQuick
import QtQuick.Window

Window {
	id: np
	color: "transparent"
	flags: Qt.FramelessWindowHint
	width: 360
	height: Math.max(1, col.implicitHeight + 16)
	visible: false
	readonly property bool want: Notifications.popups.length > 0
	onWantChanged: {
		if (want && !visible) {
			Layer.setup(np, { layer: "overlay", anchors: Layer.Top | Layer.Right, exclusive: 0, keyboard: "none",
					 scope: "chrono-notifications", margins: [8, 8, 0, 0] })
			visible = true
		} else if (!want) {
			visible = false
		}
	}
	Column {
		id: col
		x: 8; y: 8
		width: np.width - 16
		spacing: 8
		Repeater {
			model: Notifications.popups
			NotificationCard { required property var modelData; note: modelData; width: col.width }
		}
	}
}
