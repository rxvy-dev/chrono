import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

SettingsPage {
	title: qsTr("About")
	readonly property var info: Backend.systemInfo()
	RowLayout {
		spacing: 16
		Image { source: "image://preview/" + encodeURIComponent(Chrono.theme.id); sourceSize: Qt.size(240, 150); Layout.preferredWidth: 240; Layout.preferredHeight: 150 }
		ColumnLayout {
			Label { text: "Chrono " + Chrono.version; font.pointSize: 20; font.bold: true }
			Label { text: qsTr("A Wayland desktop with themes from every era."); opacity: 0.7 }
			Label { text: Chrono.connected ? qsTr("Running · theme %1").arg(Chrono.theme.name) : qsTr("Not running"); opacity: 0.7 }
		}
	}
	Section {
		title: qsTr("This computer")
		Repeater {
			model: [[qsTr("System"), info.os], [qsTr("Kernel"), info.kernel], [qsTr("Computer name"), info.host], [qsTr("Processor"), info.cpu],
				[qsTr("Memory"), info.memory], [qsTr("Graphics"), info.gpu], [qsTr("Qt"), info.qt], [qsTr("Screens"), Chrono.outputs.map(o => o.name + " " + o.mode.width + "×" + o.mode.height).join(", ")]]
			SettingRow { required property var modelData; visible: !!modelData[1]; label: modelData[0]
				Label { text: modelData[1] || ""; textFormat: Text.PlainText; horizontalAlignment: Text.AlignRight; width: 420; wrapMode: Text.Wrap } }
		}
		TextEdit { id: clip; visible: false; text: [info.os, info.kernel, info.cpu, info.memory, info.gpu, "Chrono " + Chrono.version].filter(x => x).join("\n") }
		Button { text: qsTr("Copy to clipboard"); onClicked: { clip.selectAll(); clip.copy() } }
	}
	Item { Layout.preferredHeight: 20 }
}
