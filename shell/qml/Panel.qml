// The panel: launcher button, workspaces, taskbar (unless the theme uses a dock), status area and clock.
import QtQuick
import QtQuick.Window

Window {
	id: panel
	property var shell
	readonly property var t: Chrono.theme
	readonly property bool dockMode: Chrono.dockEnabled
	readonly property string pos: Chrono.panelPosition
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

	readonly property var windowsHere: Chrono.windows.filter(w => w.workspace === Chrono.workspace || w.workspace === 0 || panel.allWorkspaces)
	readonly property var cfg: Chrono.config
	readonly property bool labels: (cfg["panel.labels"] || "true") !== "false"
	readonly property bool allWorkspaces: cfg["panel.all_workspaces"] === "true"
	// [{appId, icon, name, win}] — one entry per pinned app without windows, one per window otherwise
	readonly property var taskItems: {
		const out = []
		const used = {}
		const showPinned = (cfg["panel.pinned"] || "true") !== "false"
		const wins = panel.windowsHere
		const appOf = {}
		for (const w of wins) appOf[w.id] = Apps.forAppId(w.app_id)
		if (showPinned) {
			for (const id of Apps.pinned) {
				const a = Apps.app(id)
				if (!a.id) continue
				const mine = wins.filter(w => appOf[w.id].id === id)
				if (mine.length === 0) out.push({ appId: id, icon: a.icon, name: a.name, win: null })
				for (const w of mine) { out.push({ appId: id, icon: a.icon, name: a.name, win: w }); used[w.id] = true }
			}
		}
		for (const w of wins) {
			if (used[w.id]) continue
			const a = appOf[w.id]
			out.push({ appId: a.id || "", icon: a.icon || w.app_id || "application-x-executable", name: a.name || w.title, win: w })
		}
		return out
	}
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

		// ----- taskbar: pinned apps first (icon only until they run), then other running windows
		Row {
			id: tasks
			visible: !panel.dockMode
			anchors { left: left.right; leftMargin: 6; right: right.left; rightMargin: 6; verticalCenter: parent.verticalCenter }
			anchors.verticalCenterOffset: left.anchors.verticalCenterOffset
			spacing: t.panelStyle === "bevel" ? 3 : 4
			readonly property int iconCount: panel.taskItems.filter(i => !i.win).length
			readonly property int winCount: Math.max(1, panel.taskItems.filter(i => i.win).length)
			readonly property real iconW: bar.bh + 6
			readonly property real each: Math.max(bar.bh + 6, Math.min(panel.labels ? (t.retro ? 160 : 200) : bar.bh + 10,
							(width - iconCount * (iconW + spacing) - spacing * winCount) / winCount))
			Repeater {
				model: panel.taskItems
				PanelButton {
					id: tb
					required property var modelData
					readonly property var w: modelData.win
					height: bar.bh
					width: w ? tasks.each : tasks.iconW
					maxWidth: width
					padding: w && panel.labels && tasks.each > 70 ? 6 : (width - iconSize) / 2
					iconSize: Math.min(20, bar.bh - 6)
					icon: modelData.icon
					text: w && panel.labels && tasks.each > 70 ? w.title : ""
					checked: !!w && w.focused && !w.minimized
					opacity: w && w.minimized ? 0.6 : 1
					onClicked: w ? Chrono.windowCommand("toggle", w.id) : Apps.launch(modelData.appId)
					onMiddleClicked: modelData.appId ? Apps.launch(modelData.appId) : (w && Chrono.windowCommand("close", w.id))
					onRightClicked: (m) => panel.shell.openTaskMenu(panel.screen, mapToItem(null, m.x, 0).x, modelData)
					// running indicator for modern bars
					Rectangle {
						visible: !!tb.w && !t.retro && !tb.checked
						parent: tb
						anchors.horizontalCenter: parent.horizontalCenter
						anchors.bottom: parent.bottom; anchors.bottomMargin: 1
						width: 6; height: 2; radius: 1
						color: t.panelFg; opacity: 0.6
					}
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
			const c = Chrono.config
			const fmt = c["clock.format"] || "auto"          // auto | 12h | 24h
			const secs = c["clock.seconds"] === "true"
			let tf = fmt === "24h" ? "HH:mm" : fmt === "12h" ? "h:mm AP" : Qt.locale().timeFormat(Locale.ShortFormat)
			if (secs) tf = tf.replace("mm", "mm:ss")
			const time = Qt.formatTime(d, tf)
			const showDate = (c["clock.date"] || (t.retro ? "false" : "true")) === "true"
			text = panel.dockMode ? Qt.formatDate(d, "ddd d MMM") + "  " + time
			     : showDate ? Qt.formatDate(d, c["clock.date_format"] || "ddd d") + "  " + time : time
		}
		Component.onCompleted: update()
	}
	Timer { interval: 1000; running: true; repeat: true; onTriggered: clock.update() }
	Connections { target: Chrono; function onThemeChanged() { clock.update() } function onConfigChanged() { clock.update() } }
}
