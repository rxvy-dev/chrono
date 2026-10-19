// The launcher / start menu: type to search apps (Enter runs the first hit, or the text as a command),
// or browse by category. Retro themes get a classic banner down the side.
import QtQuick

PopupLayer {
	id: L
	property var shell
	readonly property var t: Chrono.theme
	keyboard: "exclusive"
	layerName: "overlay"
	contentWidth: 560
	contentHeight: 470
	property string query: ""
	property string category: ""
	readonly property var results: query !== "" ? Apps.search(query, 40) : Apps.inCategory(category)
	property int current: 0

	onOpened: { query = ""; field.text = ""; category = ""; current = 0; field.forceActiveFocus() }
	onResultsChanged: current = 0

	function launch(a) { Apps.launch(a.id); close() }
	function accept() {
		if (results.length > 0) launch(results[Math.min(current, results.length - 1)])
		else if (query.trim() !== "") { Apps.runCommand(query.trim()); close() }
	}

	Skin { anchors.fill: parent; kind: "popup" }

	// retro banner
	Rectangle {
		id: banner
		visible: t.retro
		width: visible ? 30 : 0
		anchors { left: parent.left; top: parent.top; bottom: parent.bottom; margins: t.panelStyle === "bevel" ? 3 : 1 }
		gradient: Gradient {
			GradientStop { position: 0; color: t.titleActive2 }
			GradientStop { position: 1; color: t.titleActive }
		}
		Text {
			transformOrigin: Item.Center
			rotation: -90
			x: (parent.width - width) / 2
			y: parent.height - width / 2 - height / 2 - 8
			text: "<b>Chrono</b> " + t.name
			textFormat: Text.StyledText
			color: t.titleActiveFg
			font.family: t.fontTitle.family; font.pointSize: t.fontUi.size + 6
		}
	}

	Item {
		anchors { left: banner.right; right: parent.right; top: parent.top; bottom: parent.bottom; margins: 10 }

		// ----- search field
		Item {
			id: search
			width: parent.width; height: 32
			Skin { anchors.fill: parent; kind: "field" }
			Glyph { id: sg; name: "search"; color: t.panelFg; width: 16; height: 16; x: 8; anchors.verticalCenter: parent.verticalCenter; opacity: 0.7 }
			TextInput {
				id: field
				anchors { left: sg.right; leftMargin: 8; right: parent.right; rightMargin: 8; verticalCenter: parent.verticalCenter }
				color: t.panelStyle === "bevel" || t.panelStyle === "pixel" ? t.windowFg : t.panelFg
				selectionColor: t.selection
				font.family: t.fontUi.family; font.pointSize: t.fontUi.size + 1
				focus: true
				clip: true
				onTextChanged: L.query = text
				Keys.onReturnPressed: L.accept()
				Keys.onEnterPressed: L.accept()
				Keys.onEscapePressed: L.close()
				Keys.onDownPressed: L.current = Math.min(L.results.length - 1, L.current + 1)
				Keys.onUpPressed: L.current = Math.max(0, L.current - 1)
				Keys.onTabPressed: L.current = Math.min(L.results.length - 1, L.current + 1)
				Text { visible: !field.text; text: qsTr("Type to search apps, or a command to run"); color: field.color; opacity: 0.5; font: field.font }
			}
		}

		// ----- categories
		Column {
			id: cats
			visible: L.query === ""
			anchors { top: search.bottom; topMargin: 10; left: parent.left; bottom: footer.top; bottomMargin: 8 }
			width: visible ? 140 : 0
			spacing: 2
			PanelButton { width: parent.width; height: 26; text: qsTr("All apps"); checked: L.category === ""; onClicked: L.category = "" }
			Repeater {
				model: Apps.categories
				PanelButton { required property var modelData; width: cats.width; height: 26
					text: modelData.name; checked: L.category === modelData.id; onClicked: L.category = modelData.id }
			}
		}

		// ----- results
		Item {
			anchors { top: search.bottom; topMargin: 10; left: cats.right; leftMargin: cats.visible ? 8 : 0; right: parent.right; bottom: footer.top; bottomMargin: 8 }
			Skin { anchors.fill: parent; kind: "well"; visible: t.panelStyle === "bevel" }
			ListView {
				id: list
				anchors.fill: parent
				anchors.margins: t.panelStyle === "bevel" ? 3 : 0
				clip: true
				model: L.results
				currentIndex: L.current
				boundsBehavior: Flickable.StopAtBounds
				highlightMoveDuration: 0
				delegate: PanelButton {
					required property var modelData
					required property int index
					width: list.width
					height: 34
					icon: modelData.icon
					iconSize: 24
					text: modelData.name
					checked: index === L.current
					onClicked: L.launch(modelData)
					onRightClicked: Apps.isPinned(modelData.id) ? Apps.unpin(modelData.id) : Apps.pin(modelData.id)
					Text {
						anchors.verticalCenter: parent.verticalCenter
						text: modelData.genericName || modelData.comment || ""
						color: parent.fg; opacity: 0.55
						elide: Text.ElideRight
						width: Math.max(0, list.width - 260)
						font.family: t.fontUi.family; font.pointSize: t.fontUi.size - 1
					}
				}
				Text {
					anchors.centerIn: parent
					visible: L.results.length === 0
					text: L.query ? qsTr("No apps found — press Enter to run “%1”").arg(L.query) : ""
					color: t.panelFg; opacity: 0.6
					font.family: t.fontUi.family; font.pointSize: t.fontUi.size
				}
			}
		}

		// ----- footer
		Item {
			id: footer
			anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
			height: 30
			Text {
				anchors.verticalCenter: parent.verticalCenter
				text: System.userName
				color: t.panelFg
				font.family: t.fontUi.family; font.pointSize: t.fontUi.size; font.bold: true
			}
			Row {
				anchors.right: parent.right
				anchors.verticalCenter: parent.verticalCenter
				spacing: 4
				PanelButton { height: 28; glyph: "settings"; text: qsTr("Settings"); onClicked: { Chrono.action("settings"); L.close() } }
				PanelButton { height: 28; glyph: "lock"; onClicked: { L.close(); Chrono.command("lock") } }
				PanelButton { height: 28; glyph: "power"; onClicked: { L.close(); L.shell.openPowerMenu() } }
			}
		}
	}
}
