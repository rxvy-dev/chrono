import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

SettingsPage {
	id: page
	title: qsTr("Sound")
	property var sinks: Backend.sinks()
	property var sources: Backend.sources()
	Connections { target: Backend; function onSoundChanged() { page.sinks = Backend.sinks(); page.sources = Backend.sources() } }

	Label { visible: !Backend.has("pactl"); wrapMode: Text.Wrap; Layout.fillWidth: true; opacity: 0.8
		text: qsTr("Install pipewire-pulse (or PulseAudio) for device switching here. Volume keys use wpctl.") }
	Section {
		title: qsTr("Output")
		Repeater {
			model: page.sinks
			RadioButton { required property var modelData; text: modelData.description; checked: modelData.default; onClicked: Backend.setDefaultSink(modelData.name) }
		}
		SettingRow { label: qsTr("Volume")
			Slider { width: 260; from: 0; to: 1.5; value: 0.5
				Component.onCompleted: value = Backend.has("wpctl") ? value : value
				onMoved: Backend.run("wpctl set-volume -l 1.5 @DEFAULT_AUDIO_SINK@ " + value.toFixed(2)) } }
		Button { text: qsTr("Test sound"); visible: Backend.has("paplay") || Backend.has("pw-play")
			onClicked: Backend.run((Backend.has("pw-play") ? "pw-play" : "paplay") + " /usr/share/sounds/freedesktop/stereo/audio-volume-change.oga") }
	}
	Section {
		title: qsTr("Input")
		Repeater {
			model: page.sources
			RadioButton { required property var modelData; text: modelData.description; checked: modelData.default; onClicked: Backend.setDefaultSource(modelData.name) }
		}
	}
	Section {
		title: qsTr("More")
		Button { text: qsTr("Open the full mixer"); visible: Backend.has("pavucontrol") || Backend.has("pwvucontrol")
			onClicked: Backend.run(Backend.has("pwvucontrol") ? "pwvucontrol" : "pavucontrol") }
	}
	Item { Layout.preferredHeight: 20 }
}
