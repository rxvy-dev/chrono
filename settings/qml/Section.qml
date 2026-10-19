import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
	id: sec
	property string title: ""
	default property alias content: box.data
	Layout.fillWidth: true
	spacing: 6
	Label { visible: sec.title !== ""; text: sec.title; font.bold: true; opacity: 0.85 }
	Frame {
		Layout.fillWidth: true
		padding: 12
		ColumnLayout { id: box; anchors.left: parent.left; anchors.right: parent.right; spacing: 10 }
	}
}
