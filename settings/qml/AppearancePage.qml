import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

SettingsPage {
	id: p
	title: qsTr("Themes")
	subtitle: qsTr("Each theme restyles everything at once: window frames, buttons, panel, menus, fonts, wallpaper and cursor.")
	property string filter: "all"
	Component.onCompleted: Chrono.refreshThemes()

	RowLayout {
		spacing: 6
		Repeater {
			model: [{ id: "all", text: qsTr("All") }, { id: "retro", text: qsTr("Retro") }, { id: "modern", text: qsTr("Modern") }]
			Button { required property var modelData; text: modelData.text; checkable: true; checked: p.filter === modelData.id; onClicked: p.filter = modelData.id }
		}
		Item { Layout.fillWidth: true }
		Label { text: qsTr("Current: %1").arg(Chrono.theme.name); opacity: 0.7 }
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
				readonly property bool current: Chrono.theme.id === modelData.id
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
			text: qsTr("Themes are plain text files. Copy one from %1 into ~/.local/share/chrono/themes/, rename it and change colours, title-bar style, button shapes, fonts, panel and wallpaper. It shows up here straight away.").arg(Chrono.theme.path.replace(/\/[^/]*$/, "")) }
		Button { text: qsTr("Open my themes folder"); onClicked: Backend.run("mkdir -p ~/.local/share/chrono/themes && xdg-open ~/.local/share/chrono/themes") }
	}
	Item { Layout.preferredHeight: 20 }
}
