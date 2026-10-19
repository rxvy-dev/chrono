import QtQuick
import QtQuick.Controls

SettingRow {
	id: c
	property string section
	property string key
	property bool defaultValue: false
	readonly property bool value: { const v = Chrono.config[(section + "." + key).toLowerCase()]; return v === undefined ? defaultValue : (v === "true" || v === "1" || v === "yes") }
	Switch {
		checked: c.value
		onToggled: Chrono.setConfig(c.section, c.key, checked ? "true" : "false")
	}
}
