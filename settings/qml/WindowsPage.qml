import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

SettingsPage {
	title: qsTr("Windows & Workspaces")
	Section {
		title: qsTr("Behaviour")
		ConfigSwitch { label: qsTr("Focus follows the mouse"); description: qsTr("The window under the pointer gets the keyboard, without clicking."); section: "windows"; key: "focus_follows_mouse"; defaultValue: false }
		ConfigCombo { label: qsTr("Double-click a title bar to"); section: "windows"; key: "double_click_title"; defaultValue: "maximize"
			options: [{ value: "maximize", text: qsTr("Maximize") }, { value: "minimize", text: qsTr("Minimize") }, { value: "none", text: qsTr("Nothing") }] }
		ConfigCombo { label: qsTr("Place new windows"); section: "windows"; key: "placement"; defaultValue: "center"
			options: [{ value: "center", text: qsTr("In the centre") }, { value: "cascade", text: qsTr("Cascading") }, { value: "cursor", text: qsTr("Under the mouse") }] }
	}
	Section {
		title: qsTr("Snapping")
		ConfigSwitch { label: qsTr("Snap windows to screen edges"); description: qsTr("Drag to an edge for half the screen, a corner for a quarter, the top to maximize."); section: "windows"; key: "snap"; defaultValue: true }
		ConfigSpin { label: qsTr("Snap distance"); section: "windows"; key: "snap_threshold"; defaultValue: 10; from: 1; to: 64; suffix: " px" }
		ConfigSpin { label: qsTr("Gaps between snapped windows"); section: "windows"; key: "gaps"; defaultValue: 0; from: 0; to: 64; suffix: " px" }
	}
	Section {
		title: qsTr("Workspaces")
		ConfigSpin { label: qsTr("Number of workspaces"); description: qsTr("Switch with Super+1…9 or Ctrl+Super+←/→."); section: "general"; key: "workspaces"; defaultValue: 4; from: 1; to: 9 }
	}
	Item { Layout.preferredHeight: 20 }
}
