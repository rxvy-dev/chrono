import QtQuick
import QtQuick.Controls

SettingRow {
	id: c
	property string section
	property string key
	property real defaultValue: 0
	property real from: 0
	property real to: 1
	property real step: 0.05
	property int decimals: 2
	readonly property real value: { const v = Chrono.config[(section + "." + key).toLowerCase()]; return v === undefined ? defaultValue : parseFloat(v) }
	Row {
		spacing: 8
		Slider {
			id: s
			width: 220
			from: c.from; to: c.to; stepSize: c.step
			value: c.value
			onMoved: commit.restart()
			Timer { id: commit; interval: 250; onTriggered: Chrono.setConfig(c.section, c.key, s.value.toFixed(c.decimals)) }
		}
		Label { width: 44; text: s.value.toFixed(c.decimals); anchors.verticalCenter: parent.verticalCenter }
	}
}
