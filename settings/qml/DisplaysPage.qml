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
		fitView()
	}
	Component.onCompleted: reload()
	Connections { target: Chrono; function onOutputsChanged() { if (!page.dirty && !confirm.visible) page.reload() } }

	function update(fn) {
		const copy = JSON.parse(JSON.stringify(outs))
		const before = rectOf(copy[sel])
		fn(copy[sel])
		const after = rectOf(copy[sel])
		// a new size (resolution, scale, rotation) must not leave gaps or overlaps: close them up
		if (before.w !== after.w || before.h !== after.h) {
			const order = copy.filter(o => o.enabled).sort((a, b) => a.x - b.x || a.y - b.y)
			const sideBySide = order.every(o => Math.abs(o.y - order[0].y) < 4)
			const stacked = order.every(o => Math.abs(o.x - order[0].x) < 4)
			if (sideBySide) { let x = order[0].x; for (const o of order) { o.x = x; x += logicalSize(o).w } }
			else if (stacked) { order.sort((a, b) => a.y - b.y); let y = order[0].y; for (const o of order) { o.y = y; y += logicalSize(o).h } }
			else for (let i = 0; i < copy.length; i++) if (i !== sel && copy[i].enabled && overlaps(rectOf(copy[i]), after)) {
				const sp = snap(i, copy[i].x, copy[i].y); copy[i].x = sp.x; copy[i].y = sp.y }
		}
		outs = copy
		dirty = true
		fitView(true)
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
		const err = applyList(normalize(JSON.parse(JSON.stringify(outs))))
		dirty = false
		if (err) { errorLabel.text = err; reload() }
		else confirm.start()
	}

	// ----- arrangement
	// ----- arrangement: drag a screen; it snaps edge-to-edge against the others (never overlapping,
	// never floating apart), and the whole layout is kept starting at 0,0 like the compositor expects.
	property int dragIndex: -1
	property point dragPos: Qt.point(0, 0)

	function rectOf(o) { const s = logicalSize(o); return { x: o.x, y: o.y, w: s.w, h: s.h } }
	function overlaps(a, b) { return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h }
	// best position for screen `i` near layout point (nx, ny): touching another screen, aligned when close
	function snap(i, nx, ny) {
		const me = logicalSize(outs[i])
		const others = []
		for (let j = 0; j < outs.length; j++) if (j !== i && outs[j].enabled) others.push(rectOf(outs[j]))
		if (!others.length) return { x: 0, y: 0 }
		const cands = []
		for (const o of others) {
			// along a vertical edge (left/right of o): x fixed, try aligning tops, bottoms, centres, or keep y
			for (const x of [o.x + o.w, o.x - me.w])
			{
				for (const y of [o.y, o.y + o.h - me.h, o.y + (o.h - me.h) / 2]) cands.push({ x: x, y: y, aligned: true })
				const ov = Math.min(me.h, o.h) * 0.25   // share at least a quarter of the edge, never just a corner
				cands.push({ x: x, y: Math.max(o.y - me.h + ov, Math.min(o.y + o.h - ov, ny)), aligned: false })
			}
			// along a horizontal edge (above/below o)
			for (const y of [o.y + o.h, o.y - me.h])
			{
				for (const x of [o.x, o.x + o.w - me.w, o.x + (o.w - me.w) / 2]) cands.push({ x: x, y: y, aligned: true })
				const ov = Math.min(me.w, o.w) * 0.25
				cands.push({ x: Math.max(o.x - me.w + ov, Math.min(o.x + o.w - ov, nx)), y: y, aligned: false })
			}
		}
		let best = null
		// lined-up spots (tops, bottoms, centres) win unless a free spot is much closer
		const bias = Math.min(me.w, me.h) * 0.35
		for (const c of cands) {
			const r = { x: c.x, y: c.y, w: me.w, h: me.h }
			if (others.some(o => overlaps(r, o))) continue
			const d = Math.hypot(c.x - nx, c.y - ny) + (c.aligned ? 0 : bias)
			if (!best || d < best.d) best = { x: Math.round(c.x), y: Math.round(c.y), d: d }
		}
		return best || { x: Math.round(nx), y: Math.round(ny) }
	}
	// while dragging: follow the mouse, but pull to another screen's edges/alignments when within `t`
	function magnet(i, nx, ny, t) {
		const me = logicalSize(outs[i])
		let bx = null, by = null
		for (let j = 0; j < outs.length; j++) {
			if (j === i || !outs[j].enabled) continue
			const o = rectOf(outs[j])
			for (const x of [o.x + o.w, o.x - me.w, o.x, o.x + o.w - me.w]) {
				const d = Math.abs(nx - x); if (d < t && (bx === null || d < bx.d)) bx = { v: x, d: d } }
			for (const y of [o.y + o.h, o.y - me.h, o.y, o.y + o.h - me.h]) {
				const d = Math.abs(ny - y); if (d < t && (by === null || d < by.d)) by = { v: y, d: d } }
		}
		return { x: Math.round(bx ? bx.v : nx), y: Math.round(by ? by.v : ny) }
	}
	// move the selected screen next to its neighbour with the arrow keys
	function nudge(dx, dy) {
		if (sel < 0 || outs.length < 2) return
		// jump past the other screens in that direction, keeping the other coordinate
		const r = rectOf(outs[sel])
		const others = outs.filter((o, j) => j !== sel && o.enabled).map(rectOf)
		if (!others.length) return
		let nx = r.x, ny = r.y
		if (dx < 0) nx = Math.min(...others.map(o => o.x)) - r.w
		if (dx > 0) nx = Math.max(...others.map(o => o.x + o.w))
		if (dy < 0) ny = Math.min(...others.map(o => o.y)) - r.h
		if (dy > 0) ny = Math.max(...others.map(o => o.y + o.h))
		if (dx !== 0 && others.every(o => ny >= o.y + o.h || ny + r.h <= o.y)) ny = others[0].y   // stay level with them
		if (dy !== 0 && others.every(o => nx >= o.x + o.w || nx + r.w <= o.x)) nx = others[0].x
		const sp = snap(sel, nx, ny)
		const copy = JSON.parse(JSON.stringify(outs))
		copy[sel].x = sp.x; copy[sel].y = sp.y
		outs = copy; dirty = true; fitView(true)
	}
	property point dropPos: Qt.point(0, 0)   // where the dragged screen lands when released
	function drop() {
		const i = dragIndex
		dragIndex = -1
		if (i < 0) return
		const copy = JSON.parse(JSON.stringify(outs))
		copy[i].x = dropPos.x; copy[i].y = dropPos.y
		outs = copy
		dirty = true
		fitView(true)
	}

	// The area shown in the editor. It stays put while you work, so the screens you aren't
	// touching never move; it only re-fits when something would end up outside it.
	property var view: ({ x: 0, y: 0, w: 1920, h: 1080 })
	function fitView(onlyIfNeeded) {
		let x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9, mw = 0, mh = 0
		for (const o of outs) { const r = rectOf(o); x0 = Math.min(x0, r.x); y0 = Math.min(y0, r.y); x1 = Math.max(x1, r.x + r.w); y1 = Math.max(y1, r.y + r.h); mw = Math.max(mw, r.w); mh = Math.max(mh, r.h) }
		if (x0 > x1) return
		if (onlyIfNeeded && x0 >= view.x && y0 >= view.y && x1 <= view.x + view.w && y1 <= view.y + view.h) return
		// room for any screen on any side, so moving one never re-zooms (and never moves the others)
		view = { x: x0 - mw, y: y0 - mh, w: x1 - x0 + 2 * mw, h: y1 - y0 + 2 * mh }
	}
	// shift everything so the top-left corner of the layout is 0,0 (only for what we send)
	function normalize(list) {
		let x0 = 1e9, y0 = 1e9
		for (const o of list) if (o.enabled) { x0 = Math.min(x0, o.x); y0 = Math.min(y0, o.y) }
		if (x0 === 1e9) return list
		for (const o of list) { o.x -= x0; o.y -= y0 }
		return list
	}

	Frame {
		Layout.fillWidth: true
		Layout.preferredHeight: 360
		padding: 0
		background: Rectangle { color: Qt.darker(palette.window, 1.12); radius: 8; border.color: palette.mid }
		Item {
			id: arena
			anchors.fill: parent
			clip: true
			readonly property var box: page.view
			readonly property real k: Math.min((width - 24) / Math.max(1, box.w), (height - 24) / Math.max(1, box.h), 0.2)
			focus: true
			Keys.onLeftPressed: page.nudge(-1, 0)
			Keys.onRightPressed: page.nudge(1, 0)
			Keys.onUpPressed: page.nudge(0, -1)
			Keys.onDownPressed: page.nudge(0, 1)
			Rectangle {   // where the screen being dragged will land
				visible: page.dragIndex >= 0 && (page.dropPos.x !== page.dragPos.x || page.dropPos.y !== page.dragPos.y)
				readonly property var size: page.dragIndex >= 0 ? page.logicalSize(page.outs[page.dragIndex]) : ({ w: 0, h: 0 })
				x: arena.ox + page.dropPos.x * arena.k; y: arena.oy + page.dropPos.y * arena.k
				width: size.w * arena.k; height: size.h * arena.k
				radius: 6; z: 3; color: "transparent"
				border.width: 2; border.color: palette.highlight
				Rectangle { anchors.fill: parent; radius: 6; color: palette.highlight; opacity: 0.15 }
			}
			readonly property real ox: (width - box.w * k) / 2 - box.x * k
			readonly property real oy: (height - box.h * k) / 2 - box.y * k
			Repeater {
				model: page.outs
				Rectangle {
					id: scr
					objectName: "screen-" + modelData.name
					required property var modelData
					required property int index
					readonly property var size: page.logicalSize(modelData)
					readonly property bool dragging: page.dragIndex === index
					readonly property real lx: dragging ? page.dragPos.x : modelData.x
					readonly property real ly: dragging ? page.dragPos.y : modelData.y
					x: arena.ox + lx * arena.k
					y: arena.oy + ly * arena.k
					z: dragging ? 2 : 1
					width: size.w * arena.k
					height: size.h * arena.k
					radius: 6
					opacity: !modelData.enabled ? 0.35 : dragging ? 0.8 : 1
					color: index === page.sel ? Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.30) : palette.base
					border.width: index === page.sel ? 2 : 1
					border.color: index === page.sel ? palette.highlight : Qt.rgba(palette.windowText.r, palette.windowText.g, palette.windowText.b, 0.35)
					Behavior on x { enabled: !scr.dragging; NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
					Behavior on y { enabled: !scr.dragging; NumberAnimation { duration: 140; easing.type: Easing.OutCubic } }
					Rectangle {   // the screen's number, like a sticker on the monitor
						x: 8; y: 8; width: 22; height: 22; radius: 11
						color: palette.highlight
						Label { anchors.centerIn: parent; text: scr.index + 1; color: palette.highlightedText; font.bold: true }
					}
					Rectangle {   // where the panel is
						visible: (Chrono.config["panel.screen"] || "auto") === scr.modelData.name
						anchors { left: parent.left; right: parent.right; bottom: Chrono.panelPosition === "top" ? undefined : parent.bottom; top: Chrono.panelPosition === "top" ? parent.top : undefined; margins: 3 }
						height: 5; radius: 2; color: palette.highlight; opacity: 0.7
					}
					Column {
						anchors.centerIn: parent
						Label { anchors.horizontalCenter: parent.horizontalCenter; text: scr.modelData.name; font.bold: true }
						Label { anchors.horizontalCenter: parent.horizontalCenter; opacity: 0.7; font.pointSize: 8
							text: scr.modelData.mode.width + "×" + scr.modelData.mode.height + (scr.modelData.mode.refresh > 0 ? " @ " + Math.round(scr.modelData.mode.refresh) + " Hz" : "") }
						Label { anchors.horizontalCenter: parent.horizontalCenter; opacity: 0.6; font.pointSize: 8; visible: scr.modelData.scale !== 1
							text: Math.round(scr.modelData.scale * 100) + "%" }
					}
					MouseArea {
						anchors.fill: parent
						preventStealing: true   // the page scrolls; don't let it steal the drag
						cursorShape: page.outs.length > 1 ? (pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor) : Qt.ArrowCursor
						property point grab
						property bool moved: false
						onPressed: (m) => { page.sel = scr.index; arena.forceActiveFocus(); grab = Qt.point(m.x, m.y); moved = false }
						onPositionChanged: (m) => {
							if (!pressed || page.outs.length < 2) return
							const p = mapToItem(arena, m.x, m.y)
							let nx = (p.x - grab.x - arena.ox) / arena.k, ny = (p.y - grab.y - arena.oy) / arena.k
							if (!moved && Math.hypot(nx - scr.modelData.x, ny - scr.modelData.y) < 20 / arena.k) return
							if (!moved) { moved = true; page.dragIndex = scr.index }
							// keep it inside the visible area so it can always be grabbed again
							const vis = { x: (-arena.ox) / arena.k, y: (-arena.oy) / arena.k, w: arena.width / arena.k, h: arena.height / arena.k }
							nx = Math.max(vis.x, Math.min(vis.x + vis.w - scr.size.w, nx))
							ny = Math.max(vis.y, Math.min(vis.y + vis.h - scr.size.h, ny))
							const mp = page.magnet(scr.index, nx, ny, 24 / arena.k)
							page.dragPos = Qt.point(mp.x, mp.y)
							const sp = page.snap(scr.index, mp.x, mp.y)
							page.dropPos = Qt.point(sp.x, sp.y)
						}
						// reset our own state first: drop() replaces the model, which recreates this delegate
						onReleased: { const was = moved; moved = false; if (was) page.drop(); else page.dragIndex = -1 }
						onCanceled: { moved = false; page.dragIndex = -1 }
					}
				}
			}
			Label {
				anchors { left: parent.left; bottom: parent.bottom; margins: 10 }
				opacity: 0.6; font.pointSize: 8
				text: page.outs.length > 1 ? qsTr("Drag a screen where you want it (it snaps to edges), or select one and use the arrow keys. Then press Apply.") : ""
			}
		}
	}
	RowLayout {
		visible: page.outs.length > 1
		Button { text: qsTr("Line up side by side"); onClicked: {
			// left to right in the current order, tops aligned
			const copy = JSON.parse(JSON.stringify(page.outs)).sort((a, b) => a.x - b.x)
			let x = 0
			for (const o of copy) { if (!o.enabled) continue; o.x = x; o.y = 0; x += page.logicalSize(o).w }
			const byName = {}; for (const o of copy) byName[o.name] = o
			page.outs = page.outs.map(o => byName[o.name]); page.dirty = true; page.fitView() } }
		Button { text: qsTr("Stack top to bottom"); onClicked: {
			const copy = JSON.parse(JSON.stringify(page.outs)).sort((a, b) => a.y - b.y)
			let y = 0
			for (const o of copy) { if (!o.enabled) continue; o.x = 0; o.y = y; y += page.logicalSize(o).h }
			const byName = {}; for (const o of copy) byName[o.name] = o
			page.outs = page.outs.map(o => byName[o.name]); page.dirty = true; page.fitView() } }
		Item { Layout.fillWidth: true }
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
