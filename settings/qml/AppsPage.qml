import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

SettingsPage {
	id: page
	title: qsTr("Apps & Startup")
	function appsFor(cat) { return Apps.inCategory(cat).map(a => ({ value: a.exec.split(" ")[0], text: a.name })) }
	Section {
		title: qsTr("Default apps")
		ConfigCombo { label: qsTr("Terminal"); description: qsTr("Super+Return / Super+T"); section: "general"; key: "terminal"; defaultValue: ""; comboWidth: 280
			options: [{ value: "", text: qsTr("Automatic (Interstellar if installed)") }].concat(page.appsFor("TerminalEmulator")) }
		ConfigCombo { label: qsTr("File manager"); description: qsTr("Super+E"); section: "general"; key: "file_manager"; defaultValue: "xdg-open ~"; comboWidth: 280
			options: [{ value: "xdg-open ~", text: qsTr("System default") }].concat(page.appsFor("FileManager")) }
		ConfigCombo { label: qsTr("Web browser"); description: qsTr("Super+B"); section: "general"; key: "browser"; defaultValue: "xdg-open https://"; comboWidth: 280
			options: [{ value: "xdg-open https://", text: qsTr("System default") }].concat(page.appsFor("WebBrowser")) }
	}
	Section {
		title: qsTr("Start with Chrono")
		ConfigText { label: qsTr("Commands"); description: qsTr("Run when you log in, separated by ;"); section: "general"; key: "autostart"; placeholder: "nm-applet; syncthing"; fieldWidth: 320 }
		Repeater {
			model: Backend.autostart()
			CheckBox { required property var modelData; text: modelData.name; checked: modelData.enabled; onToggled: Backend.setAutostart(modelData.file, checked) }
		}
	}
	Item { Layout.preferredHeight: 20 }
}
