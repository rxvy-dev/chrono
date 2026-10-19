// A soft drop shadow made of stacked translucent rounded rectangles (no graphics effects needed,
// so it works on the software renderer too). Place it behind a surface, same geometry.
import QtQuick

Item {
	id: sh
	property real radius: 12
	property int size: 18
	property real strength: 0.35
	property int offsetY: 6
	Repeater {
		model: 6
		Rectangle {
			required property int index
			readonly property real spread: sh.size * (index + 1) / 6
			x: -spread; y: -spread + sh.offsetY
			width: sh.width + spread * 2; height: sh.height + spread * 2
			radius: sh.radius + spread
			color: Qt.rgba(0, 0, 0, sh.strength / 9 * (1 - index / 6))
		}
	}
}
