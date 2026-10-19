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

	function primaryScreen() { return Qt.application.screens.length ? Qt.application.screens[0] : null }
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
		model: Qt.application.screens
		delegate: Panel { required property var modelData; screen: modelData; shell: shell }
	}
	property var docks: Instantiator {
		model: Chrono.dockEnabled ? Qt.application.screens : []
		delegate: Dock { required property var modelData; screen: modelData; shell: shell }
	}

	property var launcherWin: Launcher { id: launcher; shell: shell }
	property var quickPopup: QuickSettings { id: quick; shell: shell }
	property var calendarPopup: Calendar { id: calendar }
	property var menuPopup: Menu { id: menu }
	property var powerPopup: PowerMenu { id: power }
	property var osdWindow: Osd { id: osd }
	property var notes: NotificationPopups { }

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
