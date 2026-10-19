// Settings → Themes → Your colours: change any colour of the current theme, reset, or save the
// result as a theme of your own.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Section {
	id: sec
	title: qsTr("Your colours")
	readonly property var t: Chrono.theme
	readonly property var cfg: Chrono.config
	readonly property var rows: [
		{ key: "colors.accent", label: qsTr("Accent"), theme: "accent" },
		{ key: "colors.bg", label: qsTr("Background"), theme: "bg" },
		{ key: "colors.fg", label: qsTr("Text"), theme: "fg" },
		{ key: "colors.window_bg", label: qsTr("Window background"), theme: "windowBg" },
		{ key: "colors.window_fg", label: qsTr("Window text"), theme: "windowFg" },
		{ key: "colors.title_active", label: qsTr("Title bar"), theme: "titleActive" },
		{ key: "colors.title_active_fg", label: qsTr("Title text"), theme: "titleActiveFg" },
		{ key: "colors.title_inactive", label: qsTr("Inactive title bar"), theme: "titleInactive" },
		{ key: "colors.border_active", label: qsTr("Window border"), theme: "borderActive" },
		{ key: "colors.close_bg", label: qsTr("Close button"), theme: "closeBg" },
		{ key: "colors.panel_bg", label: qsTr("Panel"), theme: "panelBg" },
		{ key: "colors.panel_fg", label: qsTr("Panel text"), theme: "panelFg" },
		{ key: "desktop.color1", label: qsTr("Wallpaper colour 1"), theme: "desktop1" },
		{ key: "desktop.color2", label: qsTr("Wallpaper colour 2"), theme: "desktop2" },
	]
	property string editing: ""
	readonly property bool anyCustom: Backend.customColorKeys().some(k => (cfg[k] || "") !== "")

	Label { Layout.fillWidth: true; wrapMode: Text.Wrap; opacity: 0.75
		text: qsTr("Changes go on top of whichever theme you use, everywhere at once. Colours you've changed are marked with a dot.") }

	GridLayout {
		Layout.fillWidth: true
		columns: 2
		columnSpacing: 24
		rowSpacing: 6
		Repeater {
			model: sec.rows
			RowLayout {
				required property var modelData
				readonly property bool custom: (sec.cfg[modelData.key] || "") !== ""
				Layout.fillWidth: true
				spacing: 8
				Rectangle {
					Layout.preferredWidth: 30; Layout.preferredHeight: 22; radius: 4
					color: sec.t[modelData.theme] || "transparent"
					border.color: Qt.rgba(palette.windowText.r, palette.windowText.g, palette.windowText.b, 0.35)
					MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
						onClicked: { sec.editing = modelData.key; picker.openWith(sec.t[modelData.theme]) } }
				}
				Label { text: modelData.label; Layout.fillWidth: true }
				Rectangle { visible: custom; width: 6; height: 6; radius: 3; color: palette.highlight }
				ToolButton { visible: custom; text: "↺"; ToolTip.visible: hovered; ToolTip.text: qsTr("Back to the theme's colour")
					onClicked: Chrono.setConfig(modelData.key.split(".")[0], modelData.key.split(".")[1], "") }
			}
		}
	}
	ConfigCombo {
		label: qsTr("Wallpaper"); section: "desktop"; key: "wallpaper"; defaultValue: ""
		options: [{ value: "", text: qsTr("From the theme") }, { value: "solid", text: qsTr("Solid colour") }, { value: "gradient", text: qsTr("Gradient") }]
			.concat(["waves", "topo", "blobs", "material", "rings", "dunes", "circuit", "scanlines", "stars", "grid", "aurora", "hills", "checker", "dots", "weave", "pinstripe"]
				.map(p => ({ value: "pattern:" + p, text: p.charAt(0).toUpperCase() + p.slice(1) })))
	}
	RowLayout {
		Button { text: qsTr("Reset all"); enabled: sec.anyCustom; onClicked: { Backend.clearCustomColors(); Chrono.command("reload"); Chrono.setConfig("appearance", "accent", "theme") } }
		Item { Layout.fillWidth: true }
		TextField { id: newName; Layout.preferredWidth: 200; placeholderText: qsTr("Name for your theme") }
		Button {
			text: qsTr("Save as my theme")
			enabled: newName.text.trim() !== ""
			onClicked: {
				const id = Backend.saveTheme(newName.text)
				if (!id) { saved.text = qsTr("Couldn't save the theme."); return }
				Backend.clearCustomColors()
				Chrono.setConfig("appearance", "accent", "theme")
				if (Chrono.connected) Chrono.command("theme " + id); else Chrono.setConfig("general", "theme", id)
				saved.text = qsTr("Saved as ~/.config/chrono/themes/%1.theme").arg(id)
				newName.text = ""
			}
		}
	}
	Label { id: saved; visible: text !== ""; opacity: 0.7 }

	ColorPicker {
		id: picker
		onPicked: (hex) => {
			const k = sec.editing.split(".")
			Chrono.setConfig(k[0], k[1], hex)
			if (sec.editing === "colors.accent") Chrono.setConfig("appearance", "accent", "theme")
		}
	}
}
