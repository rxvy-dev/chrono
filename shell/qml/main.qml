// chrono-shell root: one panel (and dock) per screen, plus the shared popups.
import QtQuick
import QtQuick.Window

QtObject {
	id: shell
	readonly property bool launcherOpen: launcher.visible
	readonly property bool quickOpen: quick.visible
	readonly property bool calendarOpen: calendar.visible
	property alias quick: quick
	property alias calendar: calendar

	// Which monitor(s) get the panel: [panel] screen = auto (the biggest), all, or an output name like DP-1
	readonly property string screenCfg: Chrono.config["panel.screen"] || "auto"
	function biggestScreen() {
		let best = null, bestArea = -1
		for (const s of Qt.application.screens) {
			const a = s.width * s.height * s.devicePixelRatio * s.devicePixelRatio   // real pixels
			if (a > bestArea) { best = s; bestArea = a }
		}
		return best
	}
	readonly property var panelScreens: {
		const all = Qt.application.screens
		if (!all.length) return []
		if (screenCfg === "all") return all
		const named = all.find(s => s.name === screenCfg)
		return [named || biggestScreen()]
	}
	function primaryScreen() { return panelScreens.length ? panelScreens[0] : null }
	Component.onCompleted: console.info("chrono-shell: screens", Qt.application.screens.map(s => s.name + " " + s.width + "x" + s.height + "+" + s.virtualX + "+" + s.virtualY).join(", "),
					   "| panel setting:", screenCfg)
	function openPanelMenu(scr, x) {
		const items = [
			{ glyph: "down", text: qsTr("Panel at the bottom"), checked: Chrono.panelPosition === "bottom", run: () => Chrono.setConfig("panel", "position", "bottom") },
			{ glyph: "up", text: qsTr("Panel at the top"), checked: Chrono.panelPosition === "top", run: () => Chrono.setConfig("panel", "position", "top") },
			{ separator: true },
			{ glyph: "desktop", text: qsTr("On the biggest screen"), checked: screenCfg === "auto", run: () => Chrono.setConfig("panel", "screen", "auto") },
			{ glyph: "desktop", text: qsTr("On every screen"), checked: screenCfg === "all", run: () => Chrono.setConfig("panel", "screen", "all") },
		]
		if (Qt.application.screens.length > 1)
			for (const s of Qt.application.screens)
				items.push({ glyph: "desktop", text: qsTr("Only on %1").arg(s.name + (s.model ? " (" + s.model + ")" : "")), checked: screenCfg === s.name,
					     run: () => Chrono.setConfig("panel", "screen", s.name) })
		items.push({ separator: true })
		items.push({ glyph: "apps", text: Chrono.dockEnabled ? qsTr("Hide the dock") : qsTr("Show a dock"), run: () => Chrono.setConfig("panel", "dock", Chrono.dockEnabled ? "false" : "true") })
		items.push({ glyph: "settings", text: qsTr("Panel settings…"), run: () => Chrono.spawn("chrono-settings panel") })
		items.push({ glyph: "desktop", text: qsTr("Display settings…"), run: () => Chrono.spawn("chrono-settings displays") })
		menu.show(scr, x, items, qsTr("Panel"))
	}
	function toggleLauncher(scr, x) { launcher.toggle(scr || primaryScreen(), x) }
	function openPowerMenu() { power.open(primaryScreen()) }
	function showOsd(kind) { osd.show(kind) }

	function windowMenuItems(w) {
		const ws = []
		for (let i = 1; i <= Chrono.workspaceCount; i++) {
			if (i === w.workspace) continue
			ws.push({ text: qsTr("Move to workspace %1").arg(i), run: () => Chrono.command("move-to-workspace " + w.id + " " + i) })
		}
		return [
			{ glyph: "minimize", text: w.minimized ? qsTr("Restore") : qsTr("Minimize"), run: () => Chrono.windowCommand(w.minimized ? "unminimize" : "minimize", w.id) },
			{ glyph: "maximize", text: w.maximized ? qsTr("Unmaximize") : qsTr("Maximize"), checked: w.maximized, run: () => Chrono.windowCommand("maximize", w.id) },
			{ glyph: "desktop", text: qsTr("Fullscreen"), checked: w.fullscreen, run: () => Chrono.windowCommand("fullscreen", w.id) },
			{ separator: true },
		].concat(ws).concat([{ separator: true }, { glyph: "close", text: qsTr("Close"), run: () => Chrono.windowCommand("close", w.id) }])
	}
	function openTaskMenu(scr, x, item) {
		let items = []
		if (item.appId) {
			const a = Apps.app(item.appId)
			for (const act of (a.actions || [])) items.push({ text: act.name, run: () => Apps.launch(item.appId, act.id) })
			if (items.length) items.push({ separator: true })
			items.push({ glyph: "play", text: item.win ? qsTr("New window") : qsTr("Open"), run: () => Apps.launch(item.appId) })
			items.push(Apps.isPinned(item.appId) ? { glyph: "pin", text: qsTr("Unpin from bar"), run: () => Apps.unpin(item.appId) }
							     : { glyph: "pin", text: qsTr("Pin to bar"), run: () => Apps.pin(item.appId) })
			const i = Apps.pinned.indexOf(item.appId)
			if (i > 0) items.push({ glyph: "prev", text: qsTr("Move left"), run: () => Apps.movePinned(i, i - 1) })
			if (i >= 0 && i < Apps.pinned.length - 1) items.push({ glyph: "next", text: qsTr("Move right"), run: () => Apps.movePinned(i, i + 1) })
		}
		if (item.win) {
			if (items.length) items.push({ separator: true })
			items = items.concat(windowMenuItems(item.win))
		}
		menu.show(scr, x, items, item.win ? item.win.title : item.name)
	}
	function openAppMenu(scr, x, y, app) {
		let items = []
		for (const act of (app.actions || [])) items.push({ text: act.name, run: () => Apps.launch(app.id, act.id) })
		if (items.length) items.push({ separator: true })
		items.push({ glyph: "play", text: qsTr("Open"), run: () => { launcher.close(); Apps.launch(app.id) } })
		items.push(Apps.isPinned(app.id) ? { glyph: "pin", text: qsTr("Unpin from bar"), run: () => Apps.unpin(app.id) }
						 : { glyph: "pin", text: qsTr("Pin to bar"), run: () => Apps.pin(app.id) })
		menu.show(scr, x, items, app.name, y)
	}
	function openWindowMenu(scr, x, w) { menu.show(scr, x, windowMenuItems(w), w.title) }
	function openDockMenu(scr, x, item, wins) {
		let items = []
		if (item.id.indexOf("window:") !== 0) {
			const a = Apps.app(item.id)
			for (const act of (a.actions || [])) items.push({ text: act.name, run: () => Apps.launch(item.id, act.id) })
			if (items.length) items.push({ separator: true })
			items.push({ glyph: "play", text: qsTr("New window"), run: () => Apps.launch(item.id) })
			items.push(Apps.isPinned(item.id) ? { glyph: "pin", text: qsTr("Unpin from dock"), run: () => Apps.unpin(item.id) }
							   : { glyph: "pin", text: qsTr("Pin to dock"), run: () => Apps.pin(item.id) })
		}
		if (wins.length) {
			items.push({ separator: true })
			for (const w of wins) items.push({ text: w.title || item.name, run: () => Chrono.windowCommand("focus", w.id) })
			items.push({ glyph: "close", text: wins.length > 1 ? qsTr("Close all") : qsTr("Close"), run: () => { for (const w of wins) Chrono.windowCommand("close", w.id) } })
		}
		menu.show(scr, x, items, item.name)
	}

	property var panels: Instantiator {
		model: shell.panelScreens
		delegate: Panel { required property var modelData; screen: modelData; shell: shell }
	}
	property var docks: Instantiator {
		model: Chrono.dockEnabled ? shell.panelScreens : []
		delegate: Dock { required property var modelData; screen: modelData; shell: shell }
	}

	property var launcherWin: Launcher { id: launcher; shell: shell }
	property var quickPopup: QuickSettings { id: quick; shell: shell }
	property var calendarPopup: Calendar { id: calendar }
	property var menuPopup: Menu { id: menu }
	property var powerPopup: PowerMenu { id: power }
	property var osdWindow: Osd { id: osd; screen: shell.primaryScreen() }
	property var notes: NotificationPopups { screen: shell.primaryScreen() }

	property var conn: Connections {
		target: Chrono
		function onLauncherRequested() { shell.toggleLauncher(null, undefined) }
		function onPowerMenuRequested() { shell.openPowerMenu() }
		function onNotificationsRequested() { calendar.toggle(shell.primaryScreen(), calendar.screen ? calendar.screen.width - 300 : 1000) }
		function onOsdRequested(kind) { osd.show(kind) }
		function onWindowMenuRequested(info) {
			const w = Chrono.windows.find(x => x.id === info.id)
			if (w) menu.show(shell.primaryScreen(), info.x !== undefined ? info.x : 400, shell.windowMenuItems(w), w.title, info.y)
		}
		function onLocked() { launcher.close(); quick.close(); calendar.close(); menu.close(); power.close() }
	}
}
