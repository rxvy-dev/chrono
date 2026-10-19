// A small colour picker: saturation/value square, hue strip, hex field and a few presets.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
	id: picker
	property color color: "#3b82f6"
	property real h: 0.6
	property real s: 0.7
	property real v: 0.9
	signal picked(string hex)
	modal: true
	padding: 14
	anchors.centerIn: Overlay.overlay
	function openWith(c) {
		color = c
		h = Math.max(0, c.hsvHue); s = c.hsvSaturation; v = c.hsvValue
		hex.text = toHex(c)
		open()
	}
	function toHex(c) {
		const f = x => ("0" + Math.round(x * 255).toString(16)).slice(-2)
		return "#" + f(c.r) + f(c.g) + f(c.b)
	}
	readonly property color current: Qt.hsva(h, s, v, 1)
	onCurrentChanged: if (!hex.activeFocus) hex.text = toHex(current)

	ColumnLayout {
		spacing: 10
		RowLayout {
			spacing: 10
			// saturation (x) / value (y)
			Rectangle {
				id: sv
				Layout.preferredWidth: 220; Layout.preferredHeight: 160
				radius: 4
				gradient: Gradient { orientation: Gradient.Horizontal
					GradientStop { position: 0; color: "white" } GradientStop { position: 1; color: Qt.hsva(picker.h, 1, 1, 1) } }
				Rectangle { anchors.fill: parent; radius: 4
					gradient: Gradient { GradientStop { position: 0; color: "transparent" } GradientStop { position: 1; color: "black" } } }
				Rectangle { x: picker.s * sv.width - 7; y: (1 - picker.v) * sv.height - 7; width: 14; height: 14; radius: 7
					color: "transparent"; border.width: 2; border.color: picker.v > 0.5 ? "black" : "white" }
				MouseArea {
					anchors.fill: parent
					function set(m) { picker.s = Math.max(0, Math.min(1, m.x / width)); picker.v = 1 - Math.max(0, Math.min(1, m.y / height)) }
					onPressed: (m) => set(m)
					onPositionChanged: (m) => { if (pressed) set(m) }
				}
			}
			// hue
			Rectangle {
				id: hue
				Layout.preferredWidth: 20; Layout.preferredHeight: 160
				radius: 4
				gradient: Gradient {
					GradientStop { position: 0.000; color: "#ff0000" } GradientStop { position: 0.167; color: "#ffff00" }
					GradientStop { position: 0.333; color: "#00ff00" } GradientStop { position: 0.500; color: "#00ffff" }
					GradientStop { position: 0.667; color: "#0000ff" } GradientStop { position: 0.833; color: "#ff00ff" }
					GradientStop { position: 1.000; color: "#ff0000" }
				}
				Rectangle { y: picker.h * hue.height - 2; width: parent.width; height: 4; color: "white"; border.color: "black" }
				MouseArea {
					anchors.fill: parent
					function set(m) { picker.h = Math.max(0, Math.min(0.999, m.y / height)) }
					onPressed: (m) => set(m)
					onPositionChanged: (m) => { if (pressed) set(m) }
				}
			}
		}
		Flow {
			Layout.preferredWidth: 250
			spacing: 6
			Repeater {
				model: ["#000000", "#ffffff", "#808080", "#ef4444", "#f97316", "#eab308", "#22c55e", "#14b8a6", "#3b82f6", "#8b5cf6", "#ec4899", "#008080", "#000080", "#c0c0c0"]
				Rectangle { required property string modelData; width: 22; height: 22; radius: 4; color: modelData; border.color: "#80808080"
					MouseArea { anchors.fill: parent; onClicked: { const c = Qt.color(modelData); picker.h = Math.max(0, c.hsvHue); picker.s = c.hsvSaturation; picker.v = c.hsvValue } } }
			}
		}
		RowLayout {
			Rectangle { Layout.preferredWidth: 40; Layout.preferredHeight: 28; radius: 4; color: picker.current; border.color: "#80808080" }
			TextField {
				id: hex
				Layout.preferredWidth: 100
				validator: RegularExpressionValidator { regularExpression: /#?[0-9a-fA-F]{6}/ }
				onEditingFinished: if (acceptableInput) { const c = Qt.color(text.charAt(0) === "#" ? text : "#" + text)
					picker.h = Math.max(0, c.hsvHue); picker.s = c.hsvSaturation; picker.v = c.hsvValue }
			}
			Item { Layout.fillWidth: true }
			Button { text: qsTr("Cancel"); onClicked: picker.close() }
			Button { text: qsTr("Use"); highlighted: true; onClicked: { picker.picked(picker.toHex(picker.current)); picker.close() } }
		}
	}
}
