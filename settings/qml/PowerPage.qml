import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

SettingsPage {
	id: page
	title: qsTr("Power & Lock")
	readonly property var times: [{ value: "0", text: qsTr("Never") }, { value: "60", text: qsTr("1 minute") }, { value: "180", text: qsTr("3 minutes") },
		{ value: "300", text: qsTr("5 minutes") }, { value: "600", text: qsTr("10 minutes") }, { value: "900", text: qsTr("15 minutes") },
		{ value: "1800", text: qsTr("30 minutes") }, { value: "3600", text: qsTr("1 hour") }]
	Section {
		title: qsTr("When idle")
		ConfigCombo { label: qsTr("Lock the screen after"); section: "power"; key: "lock_after"; defaultValue: "600"; options: page.times }
		ConfigCombo { label: qsTr("Turn the screen off after"); section: "power"; key: "screen_off_after"; defaultValue: "900"; options: page.times }
		Label { text: qsTr("Videos and games that ask to stay awake are respected."); opacity: 0.65 }
	}
	Section {
		title: qsTr("Lock screen")
		Button { text: qsTr("Lock now"); onClicked: Chrono.command("lock") }
	}
	Item { Layout.preferredHeight: 20 }
}
