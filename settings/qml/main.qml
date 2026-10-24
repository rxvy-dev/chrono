// Chrono Settings: a sidebar of pages (searchable) and the page itself, coloured by the current theme.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
	id: win
	width: 1080
	height: 760
	minimumWidth: 720
	minimumHeight: 480
	visible: true
	title: qsTr("Chrono Settings")
	readonly property var t: Chrono.theme

	// the whole app follows the desktop theme
	palette.window: t.windowBg
	palette.windowText: t.windowFg
	palette.base: Qt.lighter(t.windowBg, t.windowBg.hslLightness < 0.5 ? 1.25 : 1.04)
	palette.alternateBase: t.windowBg
	palette.text: t.windowFg
	palette.button: t.buttonBg
	palette.buttonText: t.buttonFg
	palette.highlight: t.accent
	palette.highlightedText: t.accentFg
	palette.light: t.bevelLight
	palette.dark: t.bevelDark
	palette.mid: Qt.darker(t.windowBg, 1.3)
	palette.shadow: t.bevelShadow
	palette.placeholderText: Qt.rgba(t.windowFg.r, t.windowFg.g, t.windowFg.b, 0.5)
	font.family: t.fontUi.family
	font.pointSize: Math.max(9, t.fontUi.size)

	readonly property var pages: [
		{ id: "themes", name: qsTr("Themes"), icon: "palette", tile: "#e5484d", file: "AppearancePage.qml", keys: "appearance look colours colors style retro modern cursor icons wallpaper" },
		{ id: "panel", name: qsTr("Panel & Dock"), icon: "panel", tile: "#3b82f6", file: "PanelPage.qml", keys: "taskbar bar bottom top pin pinned dock launcher" },
		{ id: "displays", name: qsTr("Displays"), icon: "desktop", tile: "#0ea5e9", file: "DisplaysPage.qml", keys: "monitor screen resolution refresh hz scale rotation vrr freesync night light" },
		{ id: "time", name: qsTr("Time & Date"), icon: "chrono", tile: "#f59e0b", file: "TimePage.qml", keys: "clock timezone ntp 24 hour 12 date format seconds" },
		{ id: "keyboard", name: qsTr("Keyboard"), icon: "keyboard", tile: "#64748b", file: "KeyboardPage.qml", keys: "layout language repeat caps numlock" },
		{ id: "mouse", name: qsTr("Mouse & Touchpad"), icon: "mouse", tile: "#8b5cf6", file: "MousePage.qml", keys: "pointer speed acceleration scroll tap natural left handed" },
		{ id: "windows", name: qsTr("Windows"), icon: "windows", tile: "#14b8a6", file: "WindowsPage.qml", keys: "focus snap gaps workspaces placement title double click" },
		{ id: "shortcuts", name: qsTr("Shortcuts"), icon: "command", tile: "#6366f1", file: "ShortcutsPage.qml", keys: "keybinds hotkeys keys bindings" },
		{ id: "sound", name: qsTr("Sound"), icon: "volume", tile: "#ec4899", file: "SoundPage.qml", keys: "audio volume speakers headphones microphone output input" },
		{ id: "power", name: qsTr("Power & Lock"), icon: "power", tile: "#22c55e", file: "PowerPage.qml", keys: "idle lock screen off sleep suspend" },
		{ id: "apps", name: qsTr("Apps & Startup"), icon: "apps", tile: "#f97316", file: "AppsPage.qml", keys: "default terminal browser file manager autostart startup" },
		{ id: "about", name: qsTr("About"), icon: "info", tile: "#71717a", file: "AboutPage.qml", keys: "system info version hardware cpu gpu memory" },
	]
	property string current: pages.some(p => p.id === startPage) ? startPage : "themes"   // "colors" opens Themes with the colour editor

	Shortcut { sequences: [StandardKey.Quit, "Ctrl+W", "Escape"]; onActivated: win.close() }

	RowLayout {
		anchors.fill: parent
		spacing: 0

		// ----- sidebar
		Pane {
			Layout.fillHeight: true
			Layout.preferredWidth: 240
			padding: 10
			background: Rectangle { color: Qt.darker(win.palette.window, win.t.windowBg.hslLightness < 0.5 ? 0.85 : 1.04) }
			ColumnLayout {
				anchors.fill: parent
				spacing: 6
				TextField { id: search; Layout.fillWidth: true; placeholderText: qsTr("Search settings") }
				ListView {
					Layout.fillWidth: true
					Layout.fillHeight: true
					clip: true
					spacing: 2
					model: win.pages.filter(p => search.text === "" || (p.name + " " + p.keys).toLowerCase().indexOf(search.text.toLowerCase()) >= 0)
					delegate: ItemDelegate {
						required property var modelData
						width: ListView.view.width
						highlighted: win.current === modelData.id
						onClicked: win.current = modelData.id
						contentItem: RowLayout {
							spacing: 10
							Rectangle {
								Layout.preferredWidth: 26; Layout.preferredHeight: 26; radius: 7
								color: modelData.tile
								Glyph { anchors.centerIn: parent; width: 16; height: 16; name: modelData.icon; color: "white" }
							}
							Label { text: modelData.name; Layout.fillWidth: true; color: highlighted ? win.palette.highlightedText : win.palette.windowText }
						}
					}
				}
				Label { text: Chrono.connected ? "" : qsTr("Chrono isn't running: changes are saved\nand used next time you log in.")
					visible: !Chrono.connected; opacity: 0.6; font.pointSize: 8; wrapMode: Text.Wrap; Layout.fillWidth: true }
			}
		}

		// ----- page
		Loader {
			Layout.fillWidth: true
			Layout.fillHeight: true
			source: (win.pages.find(p => p.id === win.current) || win.pages[0]).file
			asynchronous: false
		}
	}
}
