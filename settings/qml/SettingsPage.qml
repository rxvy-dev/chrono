// A scrolling page with a title; put Sections inside.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ScrollView {
	id: page
	property string title: ""
	property string subtitle: ""
	default property alias content: col.data
	contentWidth: availableWidth
	clip: true
	ColumnLayout {
		id: col
		width: Math.min(page.availableWidth - 48, 820)
		x: 24
		spacing: 18
		Item { Layout.preferredHeight: 6 }
		Label { text: page.title; font.pointSize: 18; font.bold: true }
		Label { visible: page.subtitle !== ""; text: page.subtitle; opacity: 0.7; wrapMode: Text.Wrap; Layout.fillWidth: true; Layout.topMargin: -12 }
	}
}
