// Small line icons drawn in the theme's colour, so they suit every theme from 1-bit to glass.
import QtQuick

Canvas {
	id: g
	property string name: ""
	property color color: "black"
	property real value: 1.0      // level for volume / wifi / battery (0..1)
	property bool flag: false     // muted / charging
	implicitWidth: 16
	implicitHeight: 16
	onNameChanged: requestPaint()
	onColorChanged: requestPaint()
	onValueChanged: requestPaint()
	onFlagChanged: requestPaint()
	onWidthChanged: requestPaint()
	onHeightChanged: requestPaint()

	onPaint: {
		const c = getContext("2d")
		c.reset()
		const s = Math.min(width, height) / 16
		c.save()
		c.translate((width - 16 * s) / 2, (height - 16 * s) / 2)
		c.scale(s, s)
		c.strokeStyle = g.color
		c.fillStyle = g.color
		c.lineWidth = 1.6
		c.lineCap = "round"
		c.lineJoin = "round"
		const line = (pts) => { c.beginPath(); c.moveTo(pts[0], pts[1]); for (let i = 2; i < pts.length; i += 2) c.lineTo(pts[i], pts[i + 1]); c.stroke() }
		switch (g.name) {
		case "start":
			for (let x = 0; x < 2; x++) for (let y = 0; y < 2; y++) c.fillRect(2 + x * 6.5, 2 + y * 6.5, 5.5, 5.5)
			break
		case "search":
			c.beginPath(); c.arc(7, 7, 4.5, 0, Math.PI * 2); c.stroke(); line([10.5, 10.5, 14, 14]); break
		case "volume":
			c.beginPath(); c.moveTo(2, 6); c.lineTo(5, 6); c.lineTo(9, 2.5); c.lineTo(9, 13.5); c.lineTo(5, 10); c.lineTo(2, 10); c.closePath(); c.fill()
			if (g.flag) { line([11, 6, 15, 10]); line([15, 6, 11, 10]) }
			else {
				if (g.value > 0.01) { c.beginPath(); c.arc(9, 8, 3, -0.9, 0.9); c.stroke() }
				if (g.value > 0.4) { c.beginPath(); c.arc(9, 8, 5.5, -0.9, 0.9); c.stroke() }
			}
			break
		case "mic":
			c.beginPath(); c.moveTo(6, 3); c.lineTo(6, 8); c.arc(8, 8, 2, Math.PI, 0, true); c.lineTo(10, 3); c.arc(8, 3, 2, 0, Math.PI, true); c.fill()
			c.beginPath(); c.arc(8, 8, 4.5, 0, Math.PI); c.stroke(); line([8, 12.5, 8, 15])
			if (g.flag) line([2, 2, 14, 14])
			break
		case "wifi":
			for (let i = 0; i < 3; i++) {
				c.globalAlpha = g.value >= (i + 0.5) / 3.5 ? 1 : 0.3
				c.beginPath(); c.arc(8, 13.5, 3.5 + i * 3.5, -Math.PI * 0.78, -Math.PI * 0.22); c.stroke()
			}
			c.globalAlpha = 1
			c.beginPath(); c.arc(8, 13.5, 1.3, 0, Math.PI * 2); c.fill()
			break
		case "ethernet":
			c.strokeRect(5, 2, 6, 5); line([8, 7, 8, 10]); line([3, 10, 13, 10]); line([3, 10, 3, 13]); line([13, 10, 13, 13]); line([8, 10, 8, 13])
			break
		case "offline":
			line([2, 2, 14, 14]); c.globalAlpha = 0.4; c.beginPath(); c.arc(8, 13.5, 7, -Math.PI * 0.78, -Math.PI * 0.22); c.stroke(); c.globalAlpha = 1
			break
		case "battery":
			c.lineWidth = 1.3
			c.strokeRect(1.5, 4.5, 12, 7); c.fillRect(14, 6.5, 1.5, 3)
			c.fillRect(3, 6, Math.max(0.5, 9 * g.value), 4)
			if (g.flag) { c.fillStyle = g.color; c.globalCompositeOperation = "xor"; c.beginPath(); c.moveTo(8.5, 3); c.lineTo(5.5, 8.5); c.lineTo(8, 8.5); c.lineTo(7, 13); c.lineTo(10.5, 7.5); c.lineTo(8, 7.5); c.closePath(); c.fill() }
			break
		case "brightness":
			c.beginPath(); c.arc(8, 8, 3, 0, Math.PI * 2); c.fill()
			for (let i = 0; i < 8; i++) { const a = i * Math.PI / 4; line([8 + Math.cos(a) * 5, 8 + Math.sin(a) * 5, 8 + Math.cos(a) * 7, 8 + Math.sin(a) * 7]) }
			break
		case "bell":
			c.beginPath(); c.moveTo(3, 12); c.lineTo(13, 12); c.lineTo(12, 10.5); c.lineTo(12, 7); c.arc(8, 7, 4, 0, Math.PI, true); c.lineTo(4, 10.5); c.closePath(); c.stroke()
			c.beginPath(); c.arc(8, 13, 1.6, 0, Math.PI); c.fill()
			if (g.flag) line([2, 2, 14, 14])
			break
		case "power":
			c.beginPath(); c.arc(8, 8.8, 5.5, -Math.PI * 0.32, Math.PI * 1.32); c.stroke(); line([8, 1.8, 8, 8]); break
		case "reboot":
			c.beginPath(); c.arc(8, 8.5, 5.5, -Math.PI * 0.4, Math.PI * 1.25); c.stroke()
			c.beginPath(); c.moveTo(10, 1); c.lineTo(12.6, 3.6); c.lineTo(9.4, 5); c.closePath(); c.fill(); break
		case "sleep":
			c.beginPath(); c.arc(8, 8, 6, Math.PI * 0.35, Math.PI * 1.65); c.arc(10.5, 6.5, 4.5, Math.PI * 1.3, Math.PI * 0.6, true); c.closePath(); c.fill(); break
		case "lock":
			c.fillRect(3, 7.5, 10, 7); c.beginPath(); c.moveTo(5, 7.5); c.lineTo(5, 5); c.arc(8, 5, 3, Math.PI, 0); c.lineTo(11, 7.5); c.stroke(); break
		case "logout":
			line([9, 2.5, 3, 2.5, 3, 13.5, 9, 13.5]); line([6.5, 8, 14, 8]); line([11, 5, 14, 8, 11, 11]); break
		case "settings":
			c.beginPath()
			for (let i = 0; i < 16; i++) { const a = i * Math.PI / 8, r = i % 2 ? 7 : 5.2; c.lineTo(8 + Math.cos(a) * r, 8 + Math.sin(a) * r) }
			c.closePath(); c.fill()
			c.globalCompositeOperation = "destination-out"; c.beginPath(); c.arc(8, 8, 2.2, 0, Math.PI * 2); c.fill(); break
		case "close":
			line([4, 4, 12, 12]); line([12, 4, 4, 12]); break
		case "play":
			c.beginPath(); c.moveTo(4.5, 2.5); c.lineTo(13, 8); c.lineTo(4.5, 13.5); c.closePath(); c.fill(); break
		case "pause":
			c.fillRect(4, 3, 3, 10); c.fillRect(9, 3, 3, 10); break
		case "next":
			c.beginPath(); c.moveTo(3, 3); c.lineTo(10, 8); c.lineTo(3, 13); c.closePath(); c.fill(); c.fillRect(11, 3, 2, 10); break
		case "prev":
			c.beginPath(); c.moveTo(13, 3); c.lineTo(6, 8); c.lineTo(13, 13); c.closePath(); c.fill(); c.fillRect(3, 3, 2, 10); break
		case "up":
			line([3, 10.5, 8, 5.5, 13, 10.5]); break
		case "down":
			line([3, 5.5, 8, 10.5, 13, 5.5]); break
		case "right":
			line([5.5, 3, 10.5, 8, 5.5, 13]); break
		case "maximize":
			c.strokeRect(3, 3, 10, 10); break
		case "minimize":
			line([3, 12, 13, 12]); break
		case "pin":
			c.beginPath(); c.arc(8, 6, 3.5, 0, Math.PI * 2); c.fill(); line([8, 9, 8, 15]); break
		case "desktop":
			c.strokeRect(1.5, 2.5, 13, 9); line([5, 14, 11, 14]); line([8, 11.5, 8, 14]); break
		case "moon":
			c.beginPath(); c.arc(8, 8, 6, Math.PI * 0.35, Math.PI * 1.65); c.arc(10.5, 6.5, 4.5, Math.PI * 1.3, Math.PI * 0.6, true); c.closePath(); c.fill(); break
		case "trash":
			line([2.5, 4, 13.5, 4]); line([6, 4, 6.5, 2, 9.5, 2, 10, 4]); c.strokeRect(4, 4, 8, 10); break
		default:
			c.beginPath(); c.arc(8, 8, 5, 0, Math.PI * 2); c.stroke()
		}
		c.restore()
	}
}
