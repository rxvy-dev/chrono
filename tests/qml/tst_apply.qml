import QtQuick
import QtQuick.Controls
import QtTest
import "."
Item {
	id: root
	width: 1000; height: 900
	DisplaysPage { id: page; anchors.fill: parent }
	TestCase {
		name: "Apply"; when: windowShown
		function findDialog() { for (let i = 0; i < Overlay.overlay.children.length; i++) {} return null }
		function test_apply() {
			wait(200)
			// 1. position only: applies, no question asked
			page.sel = 1; page.update(o => { o.x = -1920 })
			Chrono.sent = []
			page.apply()
			wait(100)
			console.log("position-only sent:", Chrono.sent.length, "dialog visible:", page.children.length >= 0 && confirmVisible())
			verify(Chrono.sent.length === 2, "sent both outputs")
			verify(!confirmVisible(), "no keep/revert question for moving screens")
			// 2. resolution change: asks; clicking outside does NOT revert
			page.update(o => { o.mode = { width: 1280, height: 720, refresh: 60 } })
			Chrono.sent = []
			page.apply()
			wait(100)
			verify(confirmVisible(), "asks after a resolution change")
			mouseClick(root, 20, 20); wait(100)
			mouseClick(root, 900, 850); wait(100)
			keyClick(Qt.Key_Escape); wait(100)
			console.log("after clicking outside + Esc: dialog visible", confirmVisible(), "commands sent", Chrono.sent.length)
			verify(confirmVisible(), "still asking")
			compare(Chrono.sent.length, 2, "nothing reverted")
		}
		function confirmVisible() {
			const kids = Overlay.overlay.children
			for (let i = 0; i < kids.length; i++) if (kids[i].visible && kids[i].width > 50) return true
			return false
		}
	}
}
