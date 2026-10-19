// Notification popups in the top-right corner. Cards slide in, fade out, and show how long they stay.
import QtQuick
import QtQuick.Window

Window {
	id: np
	color: "transparent"
	flags: Qt.FramelessWindowHint
	width: 380
	height: Math.max(1, list.contentHeight + 24)
	visible: false

	// keep a ListModel in step with Notifications.popups so add/remove can animate
	ListModel { id: cards }
	function sync() {
		const want = Notifications.popups
		const ids = want.map(n => n.id)
		for (let i = cards.count - 1; i >= 0; i--)
			if (ids.indexOf(cards.get(i).nid) < 0) cards.remove(i)
		for (let i = 0; i < want.length; i++) {
			let at = -1
			for (let j = 0; j < cards.count; j++) if (cards.get(j).nid === want[i].id) at = j
			if (at < 0) cards.insert(Math.min(i, cards.count), { nid: want[i].id, json: JSON.stringify(want[i]) })
			else cards.setProperty(at, "json", JSON.stringify(want[i]))
		}
		if (cards.count > 0 && !visible) {
			Layer.setup(np, { layer: "overlay", anchors: 1 | 8, exclusive: 0, keyboard: "none",
					 scope: "chrono-notifications", margins: [4, 4, 0, 0] })
			visible = true
		}
		if (cards.count === 0) hideTimer.restart()
	}
	Timer { id: hideTimer; interval: 260 * Chrono.motion + 10; onTriggered: if (cards.count === 0) np.visible = false }
	Connections { target: Notifications; function onPopupsChanged() { np.sync() } }

	ListView {
		id: list
		x: 12; y: 12
		width: np.width - 24
		height: contentHeight
		spacing: 10
		interactive: false
		model: cards
		delegate: NotificationCard { required property string json; note: JSON.parse(json); width: list.width; popup: true }
		add: Transition {
			ParallelAnimation {
				NumberAnimation { property: "x"; from: 60; to: 0; duration: 220 * Chrono.motion; easing.type: Easing.OutCubic }
				NumberAnimation { property: "opacity"; from: 0; to: 1; duration: 200 * Chrono.motion }
			}
		}
		remove: Transition {
			ParallelAnimation {
				NumberAnimation { property: "opacity"; to: 0; duration: 180 * Chrono.motion }
				NumberAnimation { property: "x"; to: 40; duration: 180 * Chrono.motion; easing.type: Easing.InCubic }
			}
		}
		displaced: Transition { NumberAnimation { property: "y"; duration: 180 * Chrono.motion; easing.type: Easing.OutCubic } }
	}
}
