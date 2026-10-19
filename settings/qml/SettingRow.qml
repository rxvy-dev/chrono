// Label (+ optional description) on the left, a control on the right.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

RowLayout {
	id: r
	property string label: ""
	property string description: ""
	default property alias control: slot.data
	Layout.fillWidth: true
	spacing: 16
	ColumnLayout {
		Layout.fillWidth: true
		spacing: 1
		Label { text: r.label; Layout.fillWidth: true; wrapMode: Text.Wrap }
		Label { visible: r.description !== ""; text: r.description; opacity: 0.6; font.pointSize: Qt.application.font.pointSize - 1; Layout.fillWidth: true; wrapMode: Text.Wrap }
	}
	Item { id: slot; implicitWidth: childrenRect.width; implicitHeight: Math.max(childrenRect.height, 28); Layout.alignment: Qt.AlignVCenter }
}
