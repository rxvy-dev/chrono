// Quick settings: volume, brightness, Wi-Fi, night light, do-not-disturb, theme switcher, power.
import QtQuick

PopupLayer {
	id: Q
	property var shell
	readonly property var t: Chrono.theme
	contentWidth: 340
	contentHeight: col.implicitHeight + 24
	property var wifi: []
	property bool showWifi: false
	property bool nightOn: Chrono.configValue("night_light.enabled", "false") === "true"

	onOpened: {
		System.refreshAudio(); System.refreshBrightness()
		nightOn = Chrono.configValue("night_light.enabled", "false") === "true"
		showWifi = false
		Chrono.refreshThemes()
	}

	Skin { anchors.fill: parent; kind: "popup" }

	Column {
		id: col
		x: 12; y: 12
		width: parent.width - 24
		spacing: 10

		ThemedSlider {
			width: parent.width
			visible: System.hasAudio
			glyph: "volume"; glyphFlag: System.muted
			value: System.volume / 100
			onMoved: (v) => System.setVolume(Math.round(v * 100))
			onGlyphClicked: System.toggleMute()
		}
		ThemedSlider {
			width: parent.width
			visible: System.hasBacklight
			glyph: "brightness"
			value: System.brightness / 100
			onMoved: (v) => System.setBrightness(Math.max(1, Math.round(v * 100)))
		}

		Grid {
			width: parent.width
			columns: 2
			spacing: 6
			readonly property real cw: (width - spacing) / 2
			PanelButton {
				width: parent.cw; height: 38
				glyph: System.network === "wifi" ? "wifi" : System.network === "ethernet" ? "ethernet" : "offline"
				text: System.networkName || (System.network === "ethernet" ? qsTr("Wired") : System.network === "wifi" ? qsTr("Wi-Fi") : qsTr("Offline"))
				maxWidth: parent.cw
				checked: System.network !== "none"
				onClicked: { Q.showWifi = !Q.showWifi; if (Q.showWifi) Q.wifi = System.wifiNetworks() }
			}
			PanelButton {
				width: parent.cw; height: 38
				glyph: "moon"; text: qsTr("Night light")
				checked: Q.nightOn
				onClicked: { Q.nightOn = !Q.nightOn; Chrono.setConfig("night_light", "enabled", Q.nightOn ? "true" : "false") }
			}
			PanelButton {
				width: parent.cw; height: 38
				glyph: "bell"; text: qsTr("Do not disturb")
				checked: Notifications.doNotDisturb
				onClicked: Notifications.doNotDisturb = !Notifications.doNotDisturb
			}
			PanelButton {
				width: parent.cw; height: 38
				glyph: "mic"; text: System.micMuted ? qsTr("Mic off") : qsTr("Mic on")
				checked: !System.micMuted
				onClicked: { Chrono.action("mic mute") }
			}
		}

		// Wi-Fi list
		Column {
			visible: Q.showWifi
			width: parent.width
			spacing: 2
			Text { visible: Q.wifi.length === 0; text: System.has("nmcli") ? qsTr("No networks found") : qsTr("Install NetworkManager to manage Wi-Fi here")
				color: t.panelFg; opacity: 0.7; font.family: t.fontUi.family; font.pointSize: t.fontUi.size }
			Repeater {
				model: Q.wifi
				PanelButton {
					required property var modelData
					width: parent.width; height: 28
					glyph: "wifi"
					text: modelData.ssid + (modelData.secure ? "  🔒" : "")
					checked: modelData.active
					onClicked: System.connectWifi(modelData.ssid)
				}
			}
		}

		// theme switcher
		Text { text: qsTr("Theme"); color: t.panelFg; font.family: t.fontUi.family; font.pointSize: t.fontUi.size; font.bold: true }
		ListView {
			width: parent.width; height: 64
			orientation: ListView.Horizontal
			spacing: 6
			clip: true
			model: Chrono.themes
			delegate: Item {
				required property var modelData
				width: 96; height: 64
				Image {
					anchors.fill: parent; anchors.margins: 2
					source: "image://wallpaper/" + encodeURIComponent(modelData.id)
					sourceSize: Qt.size(96, 60)
					asynchronous: true
					cache: true
				}
				Rectangle { anchors.fill: parent; color: "transparent"; border.width: Chrono.theme.id === modelData.id ? 2 : 1
					border.color: Chrono.theme.id === modelData.id ? t.accent : t.panelBorder; radius: t.panelStyle === "bevel" ? 0 : 4 }
				Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom; margins: 2 } height: 16; color: Qt.rgba(0, 0, 0, 0.55)
					Text { anchors.centerIn: parent; text: modelData.name; color: "white"; font.pointSize: 7; font.family: t.fontUi.family } }
				MouseArea { anchors.fill: parent; onClicked: Chrono.command("theme " + modelData.id) }
			}
		}

		Row {
			spacing: 6
			anchors.right: parent.right
			PanelButton { height: 30; glyph: "settings"; text: qsTr("Settings"); onClicked: { Chrono.action("settings"); Q.close() } }
			PanelButton { height: 30; glyph: "lock"; onClicked: { Q.close(); Chrono.command("lock") } }
			PanelButton { height: 30; glyph: "power"; onClicked: { Q.close(); Q.shell.openPowerMenu() } }
		}
	}
}
