import QtQuick
import QtQuick.Controls

SettingRow {
	id: c
	property string section
	property string key
	property string defaultValue: ""
	property string placeholder: ""
	property int fieldWidth: 260
	readonly property string value: { const v = Chrono.config[(section + "." + key).toLowerCase()]; return v === undefined ? defaultValue : v }
	TextField {
		width: c.fieldWidth
		text: c.value
		placeholderText: c.placeholder
		onEditingFinished: if (text !== c.value) Chrono.setConfig(c.section, c.key, text)
	}
}
