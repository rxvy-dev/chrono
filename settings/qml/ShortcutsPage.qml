// Keyboard shortcuts: every binding (built-in and yours). Click one to change its keys or action.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

SettingsPage {
	id: page
	title: qsTr("Shortcuts")
	subtitle: qsTr("Changes take effect immediately. Built-in shortcuts can be changed or turned off too.")
	property var binds: []
	function reload() { const r = Chrono.commandSync("keybinds"); binds = Array.isArray(r) ? r : [] }
	Component.onCompleted: reload()
	Connections { target: Chrono; function onConfigChanged() { page.reload() } }

	readonly property var actions: [
		{ value: "exec terminal", text: qsTr("Open the terminal") }, { value: "exec files", text: qsTr("Open the file manager") },
		{ value: "exec browser", text: qsTr("Open the browser") }, { value: "launcher", text: qsTr("App menu") },
		{ value: "settings", text: qsTr("Settings") }, { value: "lock", text: qsTr("Lock the screen") },
		{ value: "power-menu", text: qsTr("Power menu") }, { value: "close", text: qsTr("Close window") },
		{ value: "maximize", text: qsTr("Maximize") }, { value: "minimize", text: qsTr("Minimize") },
		{ value: "fullscreen", text: qsTr("Fullscreen") }, { value: "snap left", text: qsTr("Snap left") },
		{ value: "snap right", text: qsTr("Snap right") }, { value: "switcher next", text: qsTr("Switch windows") },
		{ value: "workspace next", text: qsTr("Next workspace") }, { value: "workspace prev", text: qsTr("Previous workspace") },
		{ value: "screenshot", text: qsTr("Screenshot") }, { value: "screenshot area", text: qsTr("Screenshot of an area") },
		{ value: "screenshot window", text: qsTr("Screenshot of the window") }, { value: "reload", text: qsTr("Reload config") },
		{ value: "volume up", text: qsTr("Volume up") }, { value: "volume down", text: qsTr("Volume down") },
		{ value: "volume mute", text: qsTr("Mute") }, { value: "media play-pause", text: qsTr("Play / pause") },
		{ value: "notifications", text: qsTr("Notifications & calendar") },
	]
	function describe(a) {
		const k = actions.find(x => x.value === a)
		if (k) return k.text
		if (a.indexOf("workspace ") === 0) return qsTr("Go to workspace %1").arg(a.slice(10))
		if (a.indexOf("move-to-workspace ") === 0) return qsTr("Move window to workspace %1").arg(a.slice(18))
		if (a.indexOf("exec ") === 0) return qsTr("Run: %1").arg(a.slice(5))
		return a
	}

	RowLayout {
		TextField { id: filter; Layout.preferredWidth: 300; placeholderText: qsTr("Search shortcuts") }
		Item { Layout.fillWidth: true }
		Button { text: qsTr("Add shortcut"); onClicked: editor.edit("", "exec ") }
	}
	Section {
		Repeater {
			model: page.binds.filter(b => filter.text === "" || (b.keys + " " + page.describe(b.action)).toLowerCase().indexOf(filter.text.toLowerCase()) >= 0)
			RowLayout {
				required property var modelData
				Layout.fillWidth: true
				Label { text: page.describe(modelData.action); Layout.fillWidth: true; elide: Text.ElideRight }
				Button { text: modelData.keys; flat: true; font.family: "monospace"; onClicked: editor.edit(modelData.keys, modelData.action) }
				ToolButton { text: "✕"; ToolTip.visible: hovered; ToolTip.text: qsTr("Turn off"); onClicked: Chrono.setConfig("keybinds", modelData.keys, "none") }
			}
		}
	}
	Item { Layout.preferredHeight: 20 }

	Dialog {
		id: editor
		parent: Overlay.overlay
		anchors.centerIn: parent
		modal: true
		title: qsTr("Shortcut")
		property string oldKeys: ""
		property string keys: ""
		property bool recording: false
		// pause Chrono's own shortcuts while recording so e.g. Super+Q reaches us instead of closing the window
		onRecordingChanged: Chrono.command("inhibit-binds " + (recording && visible ? "on" : "off"))
		onClosed: Chrono.command("inhibit-binds off")
		function edit(k, a) {
			oldKeys = k; keys = k
			const known = page.actions.findIndex(x => x.value === a)
			actionBox.currentIndex = known >= 0 ? known : page.actions.length
			custom.text = known >= 0 ? "" : a
			recording = k === ""
			open()
			keyCatcher.forceActiveFocus()
		}
		standardButtons: Dialog.Save | Dialog.Cancel
		onAccepted: {
			let action = actionBox.currentIndex < page.actions.length ? page.actions[actionBox.currentIndex].value : custom.text.trim()
			if (actionBox.currentIndex >= page.actions.length && !/^(exec|workspace|move-to-workspace|snap|switcher|screenshot|volume|mic|brightness|media|theme|launcher|settings|lock|close|maximize|minimize|fullscreen|reload|power-menu|notifications)\b/.test(action))
				action = "exec " + action
			if (!keys || !action) return
			if (oldKeys && oldKeys !== keys) Chrono.setConfig("keybinds", oldKeys, "none")
			Chrono.setConfig("keybinds", keys, action)
		}
		ColumnLayout {
			spacing: 10
			Button {
				id: keyCatcher
				Layout.preferredWidth: 340
				text: editor.recording ? qsTr("Press the keys…") : (editor.keys || qsTr("Click, then press the keys"))
				font.family: "monospace"
				onClicked: { editor.recording = true; forceActiveFocus() }
				Keys.onPressed: (e) => {
					if (!editor.recording) return
					const modsOnly = [Qt.Key_Control, Qt.Key_Shift, Qt.Key_Alt, Qt.Key_Meta, Qt.Key_Super_L, Qt.Key_Super_R, Qt.Key_AltGr]
					if (modsOnly.indexOf(e.key) >= 0) return
					let m = ""
					if (e.modifiers & Qt.ControlModifier) m += "Ctrl+"
					if (e.modifiers & Qt.AltModifier) m += "Alt+"
					if (e.modifiers & Qt.ShiftModifier) m += "Shift+"
					if (e.modifiers & Qt.MetaModifier) m += "Super+"
					const names = { [Qt.Key_Return]: "Return", [Qt.Key_Enter]: "KP_Enter", [Qt.Key_Space]: "space", [Qt.Key_Tab]: "Tab",
						[Qt.Key_Backtab]: "Tab", [Qt.Key_Escape]: "Escape", [Qt.Key_Left]: "Left", [Qt.Key_Right]: "Right", [Qt.Key_Up]: "Up",
						[Qt.Key_Down]: "Down", [Qt.Key_Print]: "Print", [Qt.Key_Delete]: "Delete", [Qt.Key_Backspace]: "BackSpace",
						[Qt.Key_Home]: "Home", [Qt.Key_End]: "End", [Qt.Key_PageUp]: "Prior", [Qt.Key_PageDown]: "Next" }
					let k = names[e.key]
					if (!k && e.key >= Qt.Key_F1 && e.key <= Qt.Key_F35) k = "F" + (e.key - Qt.Key_F1 + 1)
					if (!k && e.key >= Qt.Key_0 && e.key <= Qt.Key_9) k = String.fromCharCode(e.key)
					if (!k && e.key >= Qt.Key_A && e.key <= Qt.Key_Z) k = String.fromCharCode(e.key)
					if (!k && e.text) k = e.text
					if (!k) return
					editor.keys = m + k
					editor.recording = false
					e.accepted = true
				}
			}
			Label { visible: page.binds.some(b => b.keys.toLowerCase() === editor.keys.toLowerCase() && b.keys !== editor.oldKeys); color: "#e5a000"
				text: qsTr("Already used by: %1").arg(page.describe((page.binds.find(b => b.keys.toLowerCase() === editor.keys.toLowerCase()) || {}).action || "")) }
			ComboBox {
				id: actionBox
				Layout.preferredWidth: 340
				model: page.actions.map(a => a.text).concat([qsTr("Run a command…")])
			}
			TextField { id: custom; visible: actionBox.currentIndex === page.actions.length; Layout.preferredWidth: 340
				placeholderText: qsTr("exec firefox   or any action, e.g. workspace 3") }
		}
	}
}
