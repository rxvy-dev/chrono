import QtQuick
import QtQuick.Controls

SettingRow {
	id: c
	property string section
	property string key
	property int defaultValue: 0
	property int from: 0
	property int to: 100
	property int step: 1
	property string suffix: ""
	readonly property int value: { const v = Chrono.config[(section + "." + key).toLowerCase()]; return v === undefined ? defaultValue : parseInt(v) }
	SpinBox {
		from: c.from; to: c.to; stepSize: c.step
		editable: true
		value: c.value
		textFromValue: (v) => v + c.suffix
		valueFromText: (t) => parseInt(t)
		onValueModified: Chrono.setConfig(c.section, c.key, String(value))
	}
}
