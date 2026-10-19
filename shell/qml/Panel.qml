// The panel: launcher button, workspaces, taskbar (unless the theme uses a dock), status area and clock.
import QtQuick
import QtQuick.Window

Window {
	id: panel
	property var shell
	readonly property var t: Chrono.theme
	readonly property bool dockMode: t.panelDock
	readonly property string pos: dockMode ? "top" : (t.panelPosition === "top" ? "top" : "bottom")
	readonly property bool floating: t.panelStyle === "floating" && !dockMode
	readonly property int barHeight: Math.max(20, t.panelHeight)
	readonly property int margin: floating ? 8 : 0

	color: "transparent"
	flags: Qt.FramelessWindowHint
	visible: false
	height: barHeight + margin
	width: screen ? screen.width : 800

	function configure() {
		Layer.setup(panel, {
			layer: "top", scope: "chrono-panel", keyboard: "none",
			anchors: (pos === "top" ? Layer.Top : Layer.Bottom) | Layer.Left | Layer.Right,
			exclusive: barHeight + margin,
		})
	}
	Component.onCompleted: { configure(); visible = true }
	onPosChanged: { visible = false; configure(); visible = true }
	onFloatingChanged: { visible = false; configure(); visible = true }
	onBarHeightChanged: Layer.setExclusive(panel, barHeight + margin)

	readonly property var windowsHere: Chrono.windows.filter(w => w.workspace === Chrono.workspace || w.workspace === 0)
	readonly property var focusedWindow: { const f = Chrono.windows.filter(w => w.focused); return f.length ? f[0] : null }

	function popupAt(item, popup) {
		const p = item.mapToItem(null, item.width / 2, 0)
		popup.toggle(panel.screen, p.x)
	}

	Skin {
		id: bg
		kind: "panel"
		x: panel.margin
		y: panel.pos === "top" ? panel.margin : 0
		width: panel.width - panel.margin * 2
		height: panel.barHeight
	}

	Item {
		id: bar
		anchors.fill: bg
		anchors.leftMargin: panel.floating ? 6 : 2
		anchors.rightMargin: panel.floating ? 6 : 2
		readonly property int bh: Math.max(16, height - (t.panelStyle === "bevel" ? 6 : t.panelStyle === "pixel" ? 4 : 8))

		Row {
			id: left
			anchors.verticalCenter: parent.verticalCenter
			anchors.verticalCenterOffset: t.panelStyle === "bevel" ? (panel.pos === "bottom" ? 1 : -1) : 0
			spacing: t.panelStyle === "bevel" ? 3 : 4

			// ----- launcher
			Item {
				id: startWrap
				width: start.width; height: bar.bh
				PanelButton {
					id: start
					height: parent.height
					visible: t.decor !== "meadow"
					text: t.launcherLabel
					glyph: t.launcherLabel === "" ? "start" : ""
					bold: t.retro
					padding: t.launcherLabel === "" ? 7 : 8
					checked: panel.shell && panel.shell.launcherOpen
					onClicked: panel.shell.toggleLauncher(panel.screen, startWrap.mapToItem(null, 0, 0).x + 160)
				}
				// 2000s-style green pill start button
				Rectangle {
					visible: t.decor === "meadow"
					height: bar.height; width: meadowText.implicitWidth + 44
					y: -(bar.height - bar.bh) / 2
					x: -2
					radius: 12
					gradient: Gradient {
						GradientStop { position: 0; color: meadowMa.pressed ? "#2d7a2d" : "#5eb85e" }
						GradientStop { position: 0.45; color: meadowMa.pressed ? "#226322" : "#3c9a3c" }
						GradientStop { position: 1; color: "#2f7d2f" }
					}
					Text { id: meadowText; anchors.centerIn: parent; anchors.horizontalCenterOffset: 4
						text: t.launcherLabel || "start"; color: "white"; font.italic: true; font.bold: true
						font.family: t.fontTitle.family; font.pointSize: t.fontUi.size + 3
						style: Text.Raised; styleColor: "#1a4d1a" }
					MouseArea { id: meadowMa; anchors.fill: parent
						onClicked: panel.shell.toggleLauncher(panel.screen, startWrap.mapToItem(null, 0, 0).x + 160) }
					Component.onCompleted: startWrap.width = Qt.binding(() => t.decor === "meadow" ? width : start.width)
				}
			}

			// ----- workspaces
			Row {
				visible: Chrono.workspaceCount > 1
				spacing: t.panelStyle === "bevel" ? 0 : 2
				anchors.verticalCenter: parent.verticalCenter
				Repeater {
					model: Chrono.workspaceCount
					PanelButton {
						required property int index
						height: bar.bh
						width: Math.max(height * 0.9, implicitWidth)
						text: String(index + 1)
						padding: 5
						checked: Chrono.workspace === index + 1
						attention: Chrono.windows.some(w => w.workspace === index + 1)
						onClicked: Chrono.action("workspace " + (index + 1))
						onWheel: (w) => Chrono.action(w.angleDelta.y > 0 ? "workspace prev" : "workspace next")
					}
				}
			}

			// dock themes: focused app title, like a menu bar
			Text {
				visible: panel.dockMode && panel.focusedWindow !== null
				anchors.verticalCenter: parent.verticalCenter
				leftPadding: 6
				text: panel.focusedWindow ? (Apps.forAppId(panel.focusedWindow.app_id).name || panel.focusedWindow.title) : ""
				color: t.panelFg
				font.family: t.fontUi.family; font.pointSize: t.fontUi.size; font.bold: true
			}
		}

		// ----- taskbar
		Row {
			id: tasks
			visible: !panel.dockMode
			anchors { left: left.right; leftMargin: 6; right: right.left; rightMargin: 6; verticalCenter: parent.verticalCenter }
			anchors.verticalCenterOffset: left.anchors.verticalCenterOffset
			spacing: t.panelStyle === "bevel" ? 3 : 4
			readonly property int count: Math.max(1, panel.windowsHere.length)
			readonly property real each: Math.min(t.retro ? 160 : 200, (width - spacing * (count - 1)) / count)
			Repeater {
				model: panel.windowsHere
				PanelButton {
					required property var modelData
					readonly property var appInfo: Apps.forAppId(modelData.app_id)
					height: bar.bh
					width: tasks.each
					maxWidth: tasks.each
					icon: appInfo.icon || modelData.app_id || "application-x-executable"
					text: t.panelStyle === "floating" && tasks.each < 60 ? "" : modelData.title
					checked: modelData.focused && !modelData.minimized
					opacity: modelData.minimized ? 0.6 : 1
					onClicked: Chrono.windowCommand("toggle", modelData.id)
					onMiddleClicked: Chrono.windowCommand("close", modelData.id)
					onRightClicked: (m) => panel.shell.openWindowMenu(panel.screen, mapToItem(null, m.x, 0).x, modelData)
				}
			}
		}

		// ----- status area + clock
		Item {
			id: right
			anchors.right: parent.right
			anchors.verticalCenter: parent.verticalCenter
			anchors.verticalCenterOffset: left.anchors.verticalCenterOffset
			width: tray.implicitWidth + 8
			height: bar.bh
			Skin { anchors.fill: parent; kind: "well"; visible: t.panelStyle === "bevel" }
			Row {
				id: tray
				anchors.centerIn: parent
				spacing: 2
				PanelButton {
					visible: System.mediaTitle !== ""
					height: bar.bh - 2
					glyph: System.mediaPlaying ? "pause" : "play"
					text: System.mediaTitle
					maxWidth: 180
					onClicked: System.media("play-pause")
					onWheel: (w) => System.media(w.angleDelta.y > 0 ? "previous" : "next")
					Component.onCompleted: skin.visible = Qt.binding(() => hovered)
				}
				PanelButton {
					id: statusBtn
					height: bar.bh - 2
					padding: 5
					checked: panel.shell && panel.shell.quickOpen
					onClicked: panel.popupAt(statusBtn, panel.shell.quick)
					onWheel: (w) => { System.setVolume(System.volume + (w.angleDelta.y > 0 ? 5 : -5)); panel.shell.showOsd("volume") }
					Row {
						spacing: 7
						anchors.verticalCenter: parent.verticalCenter
						Glyph { visible: Notifications.doNotDisturb; name: "bell"; flag: true; color: statusBtn.fg; width: 15; height: 15 }
						Glyph { name: System.network === "wifi" ? "wifi" : System.network === "ethernet" ? "ethernet" : "offline"
							value: System.wifiStrength / 100; color: statusBtn.fg; width: 15; height: 15 }
						Glyph { visible: System.hasAudio; name: "volume"; value: System.volume / 100; flag: System.muted; color: statusBtn.fg; width: 15; height: 15 }
						Glyph { visible: System.hasBattery; name: "battery"; value: System.battery / 100; flag: System.charging; color: statusBtn.fg; width: 17; height: 15 }
						Text { visible: System.hasBattery && !t.retro; text: System.battery + "%"; color: statusBtn.fg
							font.family: t.fontUi.family; font.pointSize: t.fontUi.size - 1; anchors.verticalCenter: parent.verticalCenter }
					}
				}
				PanelButton {
					id: clockBtn
					height: bar.bh - 2
					padding: 6
					text: clock.text
					checked: panel.shell && panel.shell.calendarOpen
					attention: Notifications.history.length > 0 && !checked && !t.retro
					onClicked: panel.popupAt(clockBtn, panel.shell.calendar)
				}
			}
		}
	}

	QtObject {
		id: clock
		property string text: ""
		function update() {
			const d = new Date()
			const tf = Qt.locale().timeFormat(Locale.ShortFormat)
			const time = Qt.formatTime(d, tf)
			text = panel.dockMode ? Qt.formatDate(d, "ddd d MMM") + "  " + time
			     : t.retro ? time : Qt.formatDate(d, "ddd d") + "  " + time
		}
		Component.onCompleted: update()
	}
	Timer { interval: 1000 * (60 - new Date().getSeconds()) + 50; running: true; repeat: false
		onTriggered: { clock.update(); interval = 60000; repeat = true; restart() } }
	Connections { target: Chrono; function onThemeChanged() { clock.update() } }
}
