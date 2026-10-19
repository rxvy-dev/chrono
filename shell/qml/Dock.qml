// The dock for modern themes: pinned apps plus running ones, with a dot under running apps.
import QtQuick
import QtQuick.Window

Window {
	id: dock
	property var shell
	readonly property var t: Chrono.theme
	readonly property int iconSize: 44
	readonly property int pad: 8

	color: "transparent"
	flags: Qt.FramelessWindowHint
	visible: false
	width: screen ? screen.width : 800
	height: iconSize + pad * 2 + 12

	Component.onCompleted: {
		Layer.setup(dock, { layer: "top", scope: "chrono-dock", keyboard: "none",
				   anchors: 2 | 4 | 8, exclusive: height - 4 })
		visible = true
	}

	// pinned ids first, then running apps that aren't pinned
	readonly property var items: {
		const out = []
		const seen = {}
		for (const id of Apps.pinned) {
			const a = Apps.app(id)
			if (!a.id) continue
			out.push({ id: a.id, icon: a.icon, name: a.name, pinned: true, appId: "" })
			seen[a.id] = out.length - 1
		}
		for (const w of Chrono.windows) {
			const a = Apps.forAppId(w.app_id)
			const key = a.id || ("window:" + w.app_id)
			if (seen[key] !== undefined) continue
			out.push({ id: key, icon: a.icon || w.app_id, name: a.name || w.title, pinned: false, appId: w.app_id })
			seen[key] = out.length - 1
		}
		return out
	}
	function windowsFor(item) {
		return Chrono.windows.filter(w => {
			if (item.appId) return w.app_id === item.appId
			const a = Apps.forAppId(w.app_id)
			return a.id === item.id
		})
	}

	Skin {
		id: plate
		kind: "popup"
		anchors.horizontalCenter: parent.horizontalCenter
		anchors.bottom: parent.bottom
		anchors.bottomMargin: 6
		height: dock.iconSize + dock.pad * 2
		width: row.width + dock.pad * 2
	}

	Row {
		id: row
		anchors.centerIn: plate
		spacing: 6
		Item {
			width: dock.iconSize; height: dock.iconSize
			Glyph { anchors.centerIn: parent; name: "start"; width: 26; height: 26; color: Chrono.theme.panelFg }
			MouseArea { anchors.fill: parent; onClicked: dock.shell.toggleLauncher(dock.screen, dock.width / 2) }
		}
		Rectangle { width: 1; height: dock.iconSize - 10; anchors.verticalCenter: parent.verticalCenter; color: t.panelBorder }
		Repeater {
			model: dock.items
			Item {
				id: cell
				required property var modelData
				readonly property var wins: dock.windowsFor(modelData)
				width: dock.iconSize; height: dock.iconSize
				Image {
					id: img
					anchors.centerIn: parent
					source: "image://icon/" + encodeURIComponent(cell.modelData.icon)
					sourceSize: Qt.size(64, 64)
					width: ma.containsMouse ? dock.iconSize * 1.15 : dock.iconSize - 4
					height: width
					smooth: true
					asynchronous: true
					Behavior on width { NumberAnimation { duration: 110; easing.type: Easing.OutCubic } }
				}
				Rectangle {
					visible: cell.wins.length > 0
					width: cell.wins.some(w => w.focused) ? 12 : 5; height: 4; radius: 2
					anchors.horizontalCenter: parent.horizontalCenter
					anchors.top: parent.bottom; anchors.topMargin: 2
					color: t.panelFg
					Behavior on width { NumberAnimation { duration: 120 } }
				}
				MouseArea {
					id: ma
					anchors.fill: parent
					hoverEnabled: true
					acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
					onClicked: (m) => {
						if (m.button === Qt.RightButton) {
							dock.shell.openDockMenu(dock.screen, cell.mapToItem(null, cell.width / 2, 0).x, cell.modelData, cell.wins)
						} else if (m.button === Qt.MiddleButton || cell.wins.length === 0) {
							if (cell.modelData.id.indexOf("window:") !== 0) Apps.launch(cell.modelData.id)
						} else {
							// cycle through this app's windows
							const f = cell.wins.findIndex(w => w.focused)
							const next = cell.wins[(f + 1) % cell.wins.length]
							Chrono.windowCommand(cell.wins.length === 1 ? "toggle" : "focus", next.id)
						}
					}
				}
			}
		}
	}
}
