import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

SettingsPage {
	id: p
	title: qsTr("Themes")
	subtitle: qsTr("Each theme restyles everything at once: window frames, buttons, panel, menus, fonts, wallpaper and cursor.")
	property string filter: "all"
	Component.onCompleted: Chrono.refreshThemes()

	readonly property string mode: Chrono.config["appearance.mode"] || "theme"
	readonly property string accent: Chrono.config["appearance.accent"] || "theme"
	property int gen: 0
	Connections { target: Chrono; function onThemeChanged() { p.gen++ } function onConfigChanged() { p.gen++ } }

	// ----- live preview + appearance controls
	RowLayout {
		Layout.fillWidth: true
		spacing: 20
		Image {
			Layout.preferredWidth: 400
			Layout.preferredHeight: 250
			source: "image://preview/" + encodeURIComponent(Chrono.selectedTheme + "?mode=" + p.mode + "&accent=" + p.accent) + "#" + p.gen
			sourceSize: Qt.size(800, 500)
			asynchronous: true
			cache: false
		}
		ColumnLayout {
			Layout.fillWidth: true
			spacing: 12
			Label { text: Chrono.theme.name; font.pointSize: 15; font.bold: true }
			Label { text: Chrono.theme.description; wrapMode: Text.Wrap; opacity: 0.75; Layout.fillWidth: true }
			RowLayout {
				spacing: 6
				Label { text: qsTr("Style"); Layout.preferredWidth: 60 }
				Repeater {
					model: [{ v: "theme", t: qsTr("As designed") }, { v: "light", t: qsTr("Light") }, { v: "dark", t: qsTr("Dark") }]
					Button { required property var modelData; text: modelData.t; checkable: true; checked: p.mode === modelData.v
						onClicked: Chrono.setConfig("appearance", "mode", modelData.v) }
				}
			}
			Label { visible: p.mode !== "theme" && !Chrono.theme.pair && Chrono.theme.variant !== p.mode; opacity: 0.6; font.pointSize: 8; wrapMode: Text.Wrap; Layout.fillWidth: true
				text: qsTr("This theme only comes in one style.") }
			RowLayout {
				spacing: 6
				Label { text: qsTr("Accent"); Layout.preferredWidth: 60 }
				Repeater {
					model: ["theme", "wallpaper", "#3b82f6", "#8b5cf6", "#ec4899", "#ef4444", "#f97316", "#eab308", "#22c55e", "#14b8a6"]
					Rectangle {
						required property string modelData
						width: 26; height: 26; radius: 13
						color: modelData === "theme" ? "transparent" : modelData === "wallpaper" ? "transparent" : modelData
						border.width: p.accent === modelData ? 3 : 1
						border.color: p.accent === modelData ? palette.windowText : Qt.rgba(palette.windowText.r, palette.windowText.g, palette.windowText.b, 0.3)
						Label { anchors.centerIn: parent; visible: modelData === "theme"; text: "T"; font.bold: true; font.pointSize: 8 }
						Rectangle { visible: modelData === "wallpaper"; anchors.fill: parent; anchors.margins: 4; radius: width / 2
							gradient: Gradient { GradientStop { position: 0; color: Chrono.theme.desktop1 } GradientStop { position: 1; color: Chrono.theme.desktop2 } } }
						MouseArea { anchors.fill: parent; onClicked: Chrono.setConfig("appearance", "accent", modelData)
							hoverEnabled: true; ToolTip.visible: containsMouse
							ToolTip.text: modelData === "theme" ? qsTr("The theme's own accent") : modelData === "wallpaper" ? qsTr("From the wallpaper") : modelData }
					}
				}
				TextField { Layout.preferredWidth: 90; placeholderText: "#rrggbb"; text: p.accent.indexOf("#") === 0 ? p.accent : ""
					validator: RegularExpressionValidator { regularExpression: /#[0-9a-fA-F]{6}/ }
					onEditingFinished: if (acceptableInput) Chrono.setConfig("appearance", "accent", text) }
			}
			ConfigSwitch { label: qsTr("Reduce motion"); description: qsTr("Turn off animations everywhere."); section: "appearance"; key: "reduce_motion"; defaultValue: false }
		}
	}

	RowLayout {
		spacing: 6
		Repeater {
			model: [{ id: "all", text: qsTr("All") }, { id: "retro", text: qsTr("Retro") }, { id: "modern", text: qsTr("Modern") }]
			Button { required property var modelData; text: modelData.text; checkable: true; checked: p.filter === modelData.id; onClicked: p.filter = modelData.id }
		}
		Item { Layout.fillWidth: true }
		Label { text: qsTr("%1 themes").arg(Chrono.themes.length); opacity: 0.7 }
	}

	GridLayout {
		Layout.fillWidth: true
		columns: Math.max(1, Math.floor(width / 260))
		columnSpacing: 14
		rowSpacing: 14
		Repeater {
			model: Chrono.themes.filter(t => p.filter === "all" || t.era === p.filter)
			Item {
				required property var modelData
				readonly property bool current: Chrono.selectedTheme === modelData.id
				Layout.fillWidth: true
				Layout.preferredHeight: width * 10 / 16 + 54
				Rectangle {
					anchors.fill: parent
					radius: 8
					color: current ? Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.18) : (ma.containsMouse ? Qt.rgba(palette.text.r, palette.text.g, palette.text.b, 0.06) : "transparent")
					border.width: current ? 2 : 1
					border.color: current ? palette.highlight : Qt.rgba(palette.text.r, palette.text.g, palette.text.b, 0.15)
				}
				Image {
					id: img
					x: 8; y: 8
					width: parent.width - 16
					height: width * 10 / 16
					source: "image://preview/" + encodeURIComponent(modelData.id)
					sourceSize: Qt.size(width * 1.5, height * 1.5)
					asynchronous: true
					cache: true
				}
				Label { anchors { left: img.left; top: img.bottom; topMargin: 6 } text: modelData.name; font.bold: true }
				Label { anchors { right: img.right; top: img.bottom; topMargin: 6 } text: modelData.era === "retro" ? qsTr("Retro") : qsTr("Modern"); opacity: 0.55 }
				Label { anchors { left: img.left; right: img.right; top: img.bottom; topMargin: 24 } text: modelData.description; elide: Text.ElideRight; opacity: 0.7; font.pointSize: Qt.application.font.pointSize - 1 }
				MouseArea {
					id: ma
					anchors.fill: parent
					hoverEnabled: true
					onClicked: {
						if (Chrono.connected) Chrono.command("theme " + modelData.id)
						else Chrono.setConfig("general", "theme", modelData.id)
					}
				}
			}
		}
	}

	Section {
		title: qsTr("Cursor & icons")
		ConfigCombo { label: qsTr("Cursor theme"); description: qsTr("Leave on “From theme” to use the one the theme picks.")
			section: "cursor"; key: "theme"; defaultValue: ""
			options: [{ value: "", text: qsTr("From theme") }].concat(Backend.cursorThemes()) }
		ConfigCombo { label: qsTr("Cursor size"); section: "cursor"; key: "size"; defaultValue: "0"
			options: [{ value: "0", text: qsTr("From theme") }, "16", "24", "32", "48", "64"] }
		ConfigCombo { label: qsTr("Icon theme"); section: "icons"; key: "theme"; defaultValue: ""
			options: [{ value: "", text: qsTr("From theme") }].concat(Backend.iconThemes()) }
	}
	Section {
		title: qsTr("Make your own")
		Label { Layout.fillWidth: true; wrapMode: Text.Wrap; opacity: 0.8
			text: qsTr("Themes are plain text files. Copy one from %1 into ~/.config/chrono/themes/, rename it and change colours, title-bar style, button shapes, fonts, panel and wallpaper. It shows up here straight away, and saving the file restyles the desktop live.").arg(Chrono.theme.path.replace(/\/[^/]*$/, "")) }
		Button { text: qsTr("Open my themes folder"); onClicked: Backend.run("mkdir -p ~/.config/chrono/themes && xdg-open ~/.config/chrono/themes") }
	}
	Item { Layout.preferredHeight: 20 }
}
