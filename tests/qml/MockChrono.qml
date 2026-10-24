pragma Singleton
import QtQuick
import "."
QtObject {
	property bool connected: true
	property var config: ({})
	property string panelPosition: "bottom"
	property var outputs: [
		{ name: "DP-1", description: "Big", enabled: true, x: 0, y: 0, scale: 1, transform: "normal", adaptive_sync: false,
		  mode: { width: 2560, height: 1440, refresh: 144 }, modes: [{ width: 2560, height: 1440, refresh: 144, preferred: true }] },
		{ name: "HDMI-A-1", description: "Small", enabled: true, x: 2560, y: 0, scale: 1, transform: "normal", adaptive_sync: false,
		  mode: { width: 1920, height: 1080, refresh: 60 }, modes: [{ width: 1920, height: 1080, refresh: 60, preferred: true }] }
	]
	function commandSync(l) { return { ok: true } }
	function setConfig(a, b, c) { return true }
}
