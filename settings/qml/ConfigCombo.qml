// options: [{value, text}] or plain strings
import QtQuick
import QtQuick.Controls

SettingRow {
	id: c
	property string section
	property string key
	property string defaultValue: ""
	property var options: []
	property int comboWidth: 240
	readonly property string value: { const v = Chrono.config[(section + "." + key).toLowerCase()]; return v === undefined ? defaultValue : v }
	readonly property var norm: options.map(o => typeof o === "string" ? { value: o, text: o } : o)
	ComboBox {
		width: c.comboWidth
		model: c.norm.map(o => o.text)
		currentIndex: Math.max(0, c.norm.findIndex(o => o.value === c.value))
		onActivated: (i) => Chrono.setConfig(c.section, c.key, c.norm[i].value)
	}
}
