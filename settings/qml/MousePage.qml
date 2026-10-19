import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

SettingsPage {
	title: qsTr("Mouse & Touchpad")
	Section {
		title: qsTr("Mouse")
		ConfigSlider { label: qsTr("Pointer speed"); section: "pointer"; key: "accel"; defaultValue: 0; from: -1; to: 1; step: 0.05 }
		ConfigCombo { label: qsTr("Acceleration"); section: "pointer"; key: "accel_profile"; defaultValue: "adaptive"
			options: [{ value: "adaptive", text: qsTr("Adaptive") }, { value: "flat", text: qsTr("Flat (no acceleration — good for games)") }] }
		ConfigSwitch { label: qsTr("Natural scrolling"); description: qsTr("Content follows your fingers, like a phone."); section: "pointer"; key: "natural_scroll"; defaultValue: false }
		ConfigSwitch { label: qsTr("Left-handed"); description: qsTr("Swap the left and right buttons."); section: "pointer"; key: "left_handed"; defaultValue: false }
		ConfigSlider { label: qsTr("Scroll speed"); section: "pointer"; key: "scroll_speed"; defaultValue: 1; from: 0.2; to: 4; step: 0.1; decimals: 1 }
	}
	Section {
		title: qsTr("Touchpad")
		ConfigSwitch { label: qsTr("Tap to click"); section: "touchpad"; key: "tap"; defaultValue: true }
		ConfigSwitch { label: qsTr("Natural scrolling"); section: "touchpad"; key: "natural_scroll"; defaultValue: true }
		ConfigSwitch { label: qsTr("Disable while typing"); section: "touchpad"; key: "disable_while_typing"; defaultValue: true }
		ConfigSlider { label: qsTr("Pointer speed"); section: "touchpad"; key: "accel"; defaultValue: 0.2; from: -1; to: 1; step: 0.05 }
	}
	Item { Layout.preferredHeight: 20 }
}
