import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

SettingsPage {
	title: qsTr("Panel & Dock")
	subtitle: qsTr("The bar with the app menu, pinned and running apps, status icons and the clock.")

	Section {
		title: qsTr("Bar")
		ConfigCombo { label: qsTr("Position"); section: "panel"; key: "position"; defaultValue: "bottom"
			options: [{ value: "bottom", text: qsTr("Bottom") }, { value: "top", text: qsTr("Top") }, { value: "theme", text: qsTr("Wherever the theme puts it") }] }
		ConfigCombo { label: qsTr("Show on"); description: qsTr("Which monitor gets the panel. Right-click the panel to change this quickly.")
			section: "panel"; key: "screen"; defaultValue: "auto"; comboWidth: 280
			options: [{ value: "auto", text: qsTr("The biggest screen") }, { value: "all", text: qsTr("Every screen") }]
				.concat(Chrono.outputs.map(o => ({ value: o.name, text: o.name + (o.description ? " — " + o.description : "") }))) }
		ConfigSwitch { label: qsTr("Show pinned apps"); description: qsTr("Pinned apps stay on the bar even when they're closed. Right-click an app to pin it.")
			section: "panel"; key: "pinned"; defaultValue: true }
		ConfigSwitch { label: qsTr("Show window titles"); description: qsTr("Off: icons only, like a dock.")
			section: "panel"; key: "labels"; defaultValue: true }
		ConfigSwitch { label: qsTr("Show windows from all workspaces"); section: "panel"; key: "all_workspaces"; defaultValue: false }
	}
	Section {
		title: qsTr("Pinned apps")
		Label { visible: Apps.pinned.length === 0; text: qsTr("Nothing pinned yet. Right-click an app in the app menu or on the bar and choose “Pin to bar”."); opacity: 0.7; wrapMode: Text.Wrap; Layout.fillWidth: true }
		Repeater {
			model: Apps.pinned
			RowLayout {
				required property string modelData
				required property int index
				readonly property var app: Apps.app(modelData)
				Layout.fillWidth: true
				Image { source: "image://icon/" + encodeURIComponent(app.icon || "application-x-executable"); sourceSize: Qt.size(24, 24); Layout.preferredWidth: 24; Layout.preferredHeight: 24 }
				Label { text: app.name || modelData; Layout.fillWidth: true }
				ToolButton { text: "↑"; enabled: index > 0; onClicked: Apps.movePinned(index, index - 1) }
				ToolButton { text: "↓"; enabled: index < Apps.pinned.length - 1; onClicked: Apps.movePinned(index, index + 1) }
				ToolButton { text: qsTr("Unpin"); onClicked: Apps.unpin(modelData) }
			}
		}
		RowLayout {
			ComboBox {
				id: addBox
				Layout.preferredWidth: 300
				readonly property var list: Apps.inCategory("").filter(a => Apps.pinned.indexOf(a.id) < 0)
				model: list.map(a => a.name)
			}
			Button { text: qsTr("Pin"); enabled: addBox.list.length > 0; onClicked: Apps.pin(addBox.list[addBox.currentIndex].id) }
		}
	}
	Section {
		title: qsTr("Dock")
		ConfigCombo { label: qsTr("Dock"); description: qsTr("A separate dock at the bottom. When it's on, the bar moves to the top.")
			section: "panel"; key: "dock"; defaultValue: "theme"
			options: [{ value: "theme", text: qsTr("If the theme has one") }, { value: "true", text: qsTr("Always") }, { value: "false", text: qsTr("Never") }] }
	}
	Item { Layout.preferredHeight: 20 }
}
