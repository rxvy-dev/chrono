// A popup from the panel (menu, calendar, quick settings...). It is a transparent layer surface covering
// the usable area of one screen; clicking outside the content or pressing Escape closes it.
import QtQuick
import QtQuick.Window

Window {
	id: pop
	property real targetX: 0            // screen x the popup should line up with
	property real targetY: -1           // >= 0: open at this y instead of against the panel
	property string edge: Chrono.panelPosition
	property int gap: Chrono.theme.panelStyle === "floating" ? 8 : 2
	property string keyboard: "ondemand"
	property string layerName: "top"
	property bool center: false
	default property alias content: holder.data
	property alias box: holder
	property int contentWidth: 300
	readonly property bool isPopupLayer: true   // lets Skin draw a frosted backdrop
	property int contentHeight: 300
	signal opened()
	signal closed()

	visible: false
	color: "transparent"
	flags: Qt.FramelessWindowHint

	function open(scr, x, y) {
		if (scr) pop.screen = scr
		targetX = x === undefined ? (pop.screen ? pop.screen.width / 2 : 0) : x
		targetY = y === undefined ? -1 : y
		if (!visible) {
			Layer.setup(pop, { layer: layerName, anchors: 15, exclusive: 0, keyboard: keyboard, scope: "chrono-popup" })
			visible = true
		}
		holder.forceActiveFocus()
		opened()
	}
	function close() {
		if (visible) {
			visible = false
			closed()
		}
	}
	function toggle(scr, x) { visible ? close() : open(scr, x) }

	MouseArea {
		anchors.fill: parent
		acceptedButtons: Qt.AllButtons
		onPressed: pop.close()
	}

	FocusScope {
		id: holder
		focus: true
		width: pop.contentWidth
		height: pop.contentHeight
		x: pop.center ? (pop.width - width) / 2
			      : Math.max(8, Math.min(pop.width - width - 8, pop.targetX - width / 2))
		y: pop.center ? (pop.height - height) / 2
			      : pop.targetY >= 0 ? Math.max(4, Math.min(pop.height - height - 4, pop.targetY))
			      : pop.edge === "top" ? pop.gap : pop.height - height - pop.gap
		Keys.onEscapePressed: pop.close()
		// swallow clicks so they don't reach the close-catcher behind
		MouseArea { anchors.fill: parent; z: -1; acceptedButtons: Qt.AllButtons }
		opacity: pop.visible ? 1 : 0
		Behavior on opacity { NumberAnimation { duration: 90 } }
	}
}
