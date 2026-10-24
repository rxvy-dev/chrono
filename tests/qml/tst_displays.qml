import QtQuick
import QtQuick.Controls
import QtTest
import "."
Item {
	id: root
	width: 1000; height: 900
	DisplaysPage { id: page; anchors.fill: parent }
	TestCase {
		name: "Displays"; when: windowShown
		function scr(n) { return findChildDeep(page, "screen-" + n) }
		function findChildDeep(it, name) {
			if (it.objectName === name) return it
			const kids = []
			if (it.children) for (let i = 0; i < it.children.length; i++) kids.push(it.children[i])
			if (it.contentItem && it.contentItem !== it) kids.push(it.contentItem)
			for (let i = 0; i < kids.length; i++) { const r = findChildDeep(kids[i], name); if (r) return r }
			return null
		}
		function drag(item, dx, dy) {   // like a real mouse: moves in window coordinates
			const ma = item.children[item.children.length - 1]
			const c = item.mapToItem(root, item.width / 2, item.height / 2)
			mousePress(ma, item.width / 2, item.height / 2)
			for (let i = 1; i <= 12; i++) { mouseMove(root, c.x + dx * i / 12, c.y + dy * i / 12); wait(5) }
			mouseRelease(root, c.x + dx, c.y + dy)
			wait(250)
		}
		function test_zshot() {
			const it = scr("HDMI-A-1"), ma = it.children[it.children.length - 1]
			const c = it.mapToItem(root, it.width / 2, it.height / 2)
			mousePress(ma, it.width / 2, it.height / 2)
			for (let i = 1; i <= 10; i++) { mouseMove(root, c.x - 30 * i, c.y + 14 * i); wait(5) }
			wait(100)
			grabImage(root).save("/tmp/shots/displays-drag.png")
			mouseRelease(root, c.x - 300, c.y + 140); wait(300)
			grabImage(root).save("/tmp/shots/displays-after.png")
		}
		function where(n) { const o = page.outs.find(o => o.name === n); return o.x + "," + o.y }
		function pos(it) { const p = it.mapToItem(null, 0, 0); return Math.round(p.x) + "," + Math.round(p.y) }
		function touching(a, b) {
			const ra = page.rectOf(a), rb = page.rectOf(b)
			const ov = page.overlaps(ra, rb)
			const touchX = (ra.x + ra.w === rb.x || rb.x + rb.w === ra.x) && ra.y < rb.y + rb.h && rb.y < ra.y + ra.h
			const touchY = (ra.y + ra.h === rb.y || rb.y + rb.h === ra.y) && ra.x < rb.x + rb.w && rb.x < ra.x + ra.w
			return !ov && (touchX || touchY)
		}
		function test_drags() {
			wait(300)
			const big = scr("DP-1"), small = scr("HDMI-A-1")
			verify(big && small, "found screens")
			const bigBefore = pos(big)
			console.log("start", where("DP-1"), where("HDMI-A-1"), "big on screen at", bigBefore)
			// 1. drag the small one to the left of the big one
			drag(small, -(small.width + big.width), 0)
			console.log("after drag left:", where("DP-1"), where("HDMI-A-1"), "big on screen at", pos(scr("DP-1")))
			compare(pos(scr("DP-1")), bigBefore, "the big screen must not move when dragging the small one")
			verify(touching(page.outs[0], page.outs[1]), "touching after drag left")
			// 2. drag it below (vertical drag — used to get stolen by the scrolling page)
			drag(scr("HDMI-A-1"), scr("DP-1").width * 0.6, scr("DP-1").height)
			console.log("after drag down:", where("DP-1"), where("HDMI-A-1"))
			compare(pos(scr("DP-1")), bigBefore, "big still unmoved")
			verify(page.outs[1].y >= 1440, "small is below")
			verify(touching(page.outs[0], page.outs[1]), "touching after drag down")
			// 3. back to the right, then drag the BIG one around several times: never stuck
			drag(scr("HDMI-A-1"), scr("DP-1").width, -scr("DP-1").height)
			console.log("after drag back right:", where("DP-1"), where("HDMI-A-1"))
			for (const [dx, dy] of [[400, 0], [0, 250], [-500, -100], [300, 300]]) {
				const before = where("DP-1")
				drag(scr("DP-1"), dx, dy)
				console.log("drag big by", dx, dy, ":", before, "->", where("DP-1"), " small:", where("HDMI-A-1"))
				verify(touching(page.outs[0], page.outs[1]), "touching")
			}
			verify(page.outs.every(o => page.rectOf(o).x >= page.view.x && page.rectOf(o).x + page.rectOf(o).w <= page.view.x + page.view.w), "inside view")
			// 4. arrow keys
			mouseClick(scr("HDMI-A-1").children[scr("HDMI-A-1").children.length - 1], 5, 5)
			keyClick(Qt.Key_Left); wait(200)
			console.log("arrow left:", where("DP-1"), where("HDMI-A-1"))
			verify(page.outs[1].x + 1920 === page.outs[0].x, "small left of big after arrow")
		}
	}
}
