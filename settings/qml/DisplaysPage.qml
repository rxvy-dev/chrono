// Displays: drag screens to arrange them; per screen resolution, refresh rate, scale, rotation,
// variable refresh rate and on/off. Changes revert automatically unless you keep them.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

SettingsPage {
	id: page
	title: qsTr("Displays")
	subtitle: qsTr("Drag the screens to match how they sit on your desk. Click one to change it.")

	property var outs: []          // working copy
	property var original: []      // as they were before Apply
	property int sel: 0
	readonly property var cur: outs.length ? outs[Math.min(sel, outs.length - 1)] : null
	property bool dirty: false

	function reload() {
		outs = JSON.parse(JSON.stringify(Chrono.outputs))
		dirty = false
	}
	Component.onCompleted: reload()
	Connections { target: Chrono; function onOutputsChanged() { if (!page.dirty && !confirm.visible) page.reload() } }

	function update(fn) {
		const copy = JSON.parse(JSON.stringify(outs))
		fn(copy[sel])
		outs = copy
		dirty = true
	}
	function logicalSize(o) {
		const rot = o.transform === "90" || o.transform === "270" || o.transform === "flipped-90" || o.transform === "flipped-270"
		const w = (rot ? o.mode.height : o.mode.width) / o.scale, h = (rot ? o.mode.width : o.mode.height) / o.scale
		return { w: Math.round(w), h: Math.round(h) }
	}
	function settingsFor(o) {
		let s = "enabled=" + o.enabled
		if (o.enabled) {
			s += " mode=" + o.mode.width + "x" + o.mode.height + (o.mode.refresh > 0 ? "@" + o.mode.refresh.toFixed(3) : "")
			s += " scale=" + o.scale + " transform=" + o.transform + " x=" + Math.round(o.x) + " y=" + Math.round(o.y)
			s += " adaptive_sync=" + o.adaptive_sync
		}
		return s
	}
	function applyList(list) {
		let err = ""
		for (const o of list) {
			const r = Chrono.commandSync("output " + o.name + " " + settingsFor(o))
			if (!r.ok) err += o.name + ": " + (r.error || "failed") + "\n"
		}
		return err
	}
	function apply() {
		original = JSON.parse(JSON.stringify(Chrono.outputs))
		const err = applyList(outs)
		dirty = false
		if (err) { errorLabel.text = err; reload() }
		else confirm.start()
	}

	// ----- arrangement
	Frame {
		Layout.fillWidth: true
		Layout.preferredHeight: 280
		padding: 0
		Item {
			id: arena
			anchors.fill: parent
			clip: true
			readonly property var box: {
				let x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9
				for (const o of page.outs) { const s = page.logicalSize(o); x0 = Math.min(x0, o.x); y0 = Math.min(y0, o.y); x1 = Math.max(x1, o.x + s.w); y1 = Math.max(y1, o.y + s.h) }
				if (x0 > x1) return { x: 0, y: 0, w: 1920, h: 1080 }
				return { x: x0, y: y0, w: x1 - x0, h: y1 - y0 }
			}
			readonly property real k: Math.min((width - 60) / Math.max(1, box.w), (height - 60) / Math.max(1, box.h), 0.25)
			readonly property real ox: (width - box.w * k) / 2 - box.x * k
			readonly property real oy: (height - box.h * k) / 2 - box.y * k
			Repeater {
				model: page.outs
				Rectangle {
					id: scr
					required property var modelData
					required property int index
					readonly property var size: page.logicalSize(modelData)
					x: arena.ox + modelData.x * arena.k
					y: arena.oy + modelData.y * arena.k
					width: size.w * arena.k
					height: size.h * arena.k
					radius: 4
					opacity: modelData.enabled ? 1 : 0.4
					color: index === page.sel ? Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.35) : palette.button
					border.width: index === page.sel ? 2 : 1
					border.color: index === page.sel ? palette.highlight : palette.mid
					Column {
						anchors.centerIn: parent
						Label { anchors.horizontalCenter: parent.horizontalCenter; text: scr.modelData.name; font.bold: true }
						Label { anchors.horizontalCenter: parent.horizontalCenter; text: scr.modelData.mode.width + "×" + scr.modelData.mode.height; opacity: 0.7; font.pointSize: 8 }
					}
					MouseArea {
						anchors.fill: parent
						drag.target: page.outs.length > 1 ? scr : null
						drag.threshold: 2
						onPressed: page.sel = scr.index
						onReleased: {
							if (!drag.active && Math.abs(scr.x - (arena.ox + scr.modelData.x * arena.k)) < 1) return
							// back to layout coordinates, then snap to the nearest edge of another screen
							let nx = (scr.x - arena.ox) / arena.k, ny = (scr.y - arena.oy) / arena.k
							const me = scr.size
							let best = null
							for (let i = 0; i < page.outs.length; i++) {
								if (i === scr.index) continue
								const o = page.outs[i], s = page.logicalSize(o)
								const cands = [
									{ x: o.x + s.w, y: ny }, { x: o.x - me.w, y: ny },
									{ x: nx, y: o.y + s.h }, { x: nx, y: o.y - me.h },
								]
								for (const c of cands) {
									// align edges when close
									if (Math.abs(c.y - o.y) < 120) c.y = o.y
									if (Math.abs(c.x - o.x) < 120) c.x = o.x
									const d = Math.hypot(c.x - nx, c.y - ny)
									if (!best || d < best.d) best = { x: c.x, y: c.y, d: d }
								}
							}
							if (best) { nx = best.x; ny = best.y }
							page.update(o => { o.x = Math.round(nx); o.y = Math.round(ny) })
						}
					}
				}
			}
		}
	}

	// ----- selected screen
	Section {
		visible: page.cur !== null
		title: page.cur ? page.cur.name + (page.cur.description ? " — " + page.cur.description : "") : ""
		SettingRow {
			label: qsTr("Panel")
			description: qsTr("Put the panel on this screen.")
			Button {
				readonly property string cfg: Chrono.config["panel.screen"] || "auto"
				text: page.cur && cfg === page.cur.name ? qsTr("Panel is here") : qsTr("Move panel here")
				enabled: page.cur && cfg !== page.cur.name
				onClicked: Chrono.setConfig("panel", "screen", page.cur.name)
			}
		}
		SettingRow { label: qsTr("Enabled")
			Switch { checked: page.cur ? page.cur.enabled : false; onToggled: page.update(o => o.enabled = checked) } }
		SettingRow {
			label: qsTr("Resolution")
			ComboBox {
				id: resBox
				width: 240
				readonly property var sizes: {
					if (!page.cur) return []
					const seen = {}, out = []
					for (const m of page.cur.modes) { const k = m.width + "x" + m.height; if (!seen[k]) { seen[k] = true; out.push({ w: m.width, h: m.height, pref: m.preferred }) } }
					if (!out.length) out.push({ w: page.cur.mode.width, h: page.cur.mode.height, pref: true })
					out.sort((a, b) => b.w * b.h - a.w * a.h)
					return out
				}
				model: sizes.map(s => s.w + " × " + s.h + (s.pref ? qsTr("  (recommended)") : ""))
				currentIndex: page.cur ? Math.max(0, sizes.findIndex(s => s.w === page.cur.mode.width && s.h === page.cur.mode.height)) : 0
				onActivated: (i) => page.update(o => {
					const s = sizes[i]
					const rates = o.modes.filter(m => m.width === s.w && m.height === s.h).sort((a, b) => b.refresh - a.refresh)
					o.mode = { width: s.w, height: s.h, refresh: rates.length ? rates[0].refresh : 0 }
				})
			}
		}
		SettingRow {
			label: qsTr("Refresh rate")
			ComboBox {
				width: 240
				readonly property var rates: page.cur ? page.cur.modes.filter(m => m.width === page.cur.mode.width && m.height === page.cur.mode.height).map(m => m.refresh).sort((a, b) => b - a) : []
				enabled: rates.length > 1
				model: rates.length ? rates.map(r => r.toFixed(2) + " Hz") : [page.cur && page.cur.mode.refresh > 0 ? page.cur.mode.refresh.toFixed(2) + " Hz" : qsTr("Automatic")]
				currentIndex: page.cur ? Math.max(0, rates.findIndex(r => Math.abs(r - page.cur.mode.refresh) < 0.01)) : 0
				onActivated: (i) => { if (rates.length) page.update(o => o.mode.refresh = rates[i]) }
			}
		}
		SettingRow {
			label: qsTr("Scale")
			description: qsTr("Makes everything bigger on high-resolution screens. Fractional scales are supported.")
			Row {
				spacing: 8
				ComboBox {
					id: scaleBox
					width: 120
					readonly property var presets: [1, 1.25, 1.5, 1.75, 2, 2.5, 3]
					model: presets.map(s => Math.round(s * 100) + "%")
					currentIndex: page.cur ? presets.findIndex(s => Math.abs(s - page.cur.scale) < 0.01) : 0
					displayText: page.cur ? Math.round(page.cur.scale * 100) + "%" : ""
					onActivated: (i) => page.update(o => o.scale = presets[i])
				}
				SpinBox {
					from: 50; to: 400; stepSize: 5; editable: true
					value: page.cur ? Math.round(page.cur.scale * 100) : 100
					textFromValue: (v) => v + "%"
					valueFromText: (t) => parseInt(t)
					onValueModified: page.update(o => o.scale = value / 100)
				}
			}
		}
		SettingRow {
			label: qsTr("Rotation")
			ComboBox {
				width: 240
				readonly property var vals: ["normal", "90", "180", "270", "flipped", "flipped-90", "flipped-180", "flipped-270"]
				model: [qsTr("Normal"), qsTr("90° clockwise"), qsTr("Upside down"), qsTr("90° counter-clockwise"),
					qsTr("Flipped"), qsTr("Flipped, 90°"), qsTr("Flipped, 180°"), qsTr("Flipped, 270°")]
				currentIndex: page.cur ? Math.max(0, vals.indexOf(page.cur.transform)) : 0
				onActivated: (i) => page.update(o => o.transform = vals[i])
			}
		}
		SettingRow { label: qsTr("Variable refresh rate (FreeSync / G-Sync)"); description: qsTr("Smoother games; some screens flicker with it.")
			Switch { checked: page.cur ? page.cur.adaptive_sync : false; onToggled: page.update(o => o.adaptive_sync = checked) } }
		SettingRow { label: qsTr("Position"); description: qsTr("In logical pixels; dragging above sets this too.")
			Row { spacing: 6
				SpinBox { from: -32768; to: 32768; editable: true; value: page.cur ? page.cur.x : 0; onValueModified: page.update(o => o.x = value) }
				SpinBox { from: -32768; to: 32768; editable: true; value: page.cur ? page.cur.y : 0; onValueModified: page.update(o => o.y = value) } } }
		Label { visible: page.cur && page.cur.physical_width > 0; opacity: 0.6
			text: page.cur ? qsTr("Physical size: %1 × %2 mm (%3″)").arg(page.cur.physical_width).arg(page.cur.physical_height)
				.arg((Math.hypot(page.cur.physical_width, page.cur.physical_height) / 25.4).toFixed(1)) : "" }
	}

	Label { id: errorLabel; visible: text !== ""; color: "#e5484d"; wrapMode: Text.Wrap; Layout.fillWidth: true }
	RowLayout {
		Item { Layout.fillWidth: true }
		Button { text: qsTr("Reset"); enabled: page.dirty; onClicked: page.reload() }
		Button { text: qsTr("Apply"); enabled: page.dirty && Chrono.connected; highlighted: true; onClicked: page.apply() }
	}
	Label { visible: !Chrono.connected; text: qsTr("Chrono isn't running, so screens can't be changed from here."); opacity: 0.7 }

	// ----- night light
	Section {
		title: qsTr("Night light")
		ConfigSwitch { label: qsTr("Night light"); description: qsTr("Warmer colours in the evening to go easy on your eyes.")
			section: "night_light"; key: "enabled"; defaultValue: false }
		ConfigSlider { label: qsTr("Colour temperature (K)"); section: "night_light"; key: "temperature"; defaultValue: 4000; from: 1500; to: 6500; step: 100; decimals: 0 }
		ConfigText { label: qsTr("From"); description: qsTr("24-hour time, e.g. 20:00. Same From and To means always on."); section: "night_light"; key: "from"; defaultValue: "20:00"; fieldWidth: 100 }
		ConfigText { label: qsTr("To"); section: "night_light"; key: "to"; defaultValue: "07:00"; fieldWidth: 100 }
	}
	Item { Layout.preferredHeight: 20 }

	// ----- keep / revert
	Dialog {
		id: confirm
		property int left: 15
		parent: Overlay.overlay
		anchors.centerIn: parent
		modal: true
		title: qsTr("Keep these display settings?")
		Label { text: qsTr("Reverting in %1 seconds…").arg(confirm.left) }
		standardButtons: Dialog.Ok | Dialog.Cancel
		function start() { left = 15; open(); tick.start() }
		Timer { id: tick; interval: 1000; repeat: true
			onTriggered: { confirm.left--; if (confirm.left <= 0) { stop(); confirm.reject() } } }
		onAccepted: { tick.stop(); page.reload() }
		onRejected: { tick.stop(); page.applyList(page.original); page.reload() }
		Component.onCompleted: standardButton(Dialog.Ok).text = qsTr("Keep")
	}
}
