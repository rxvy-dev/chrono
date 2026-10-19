// The app menu: type to search (Enter runs the first hit, or the text as a command), or browse by
// category. Laid out on an 8-px grid: 16 padding, 40-px search, 168-px category column, 48-px rows.
// Retro themes get a banner down the side with the Chrono mark.
import QtQuick

PopupLayer {
	id: launcher
	property var shell
	readonly property var t: Chrono.theme
	readonly property bool retroLook: t.panelStyle === "bevel" || t.panelStyle === "pixel"
	// 2000s menu: white body between a glossy header (with you in it) and a glossy footer
	readonly property bool xp: t.decor === "meadow"
	readonly property int headerH: xp ? 60 : 0
	readonly property int pad: 16
	keyboard: "exclusive"
	layerName: "overlay"
	contentWidth: 600
	contentHeight: 520
	property string query: ""
	property string category: ""
	readonly property var results: query !== "" ? Apps.search(query, 40)
				     : category === "__pinned" ? Apps.pinned.map(id => Apps.app(id)).filter(a => a.id)
				     : Apps.inCategory(category)
	property int current: 0

	onOpened: {
		query = ""; field.text = ""; current = 0
		category = Apps.pinned.length ? "__pinned" : ""
		field.forceActiveFocus()
		openAnim.restart()
	}
	onResultsChanged: current = 0

	function launch(a) { Apps.launch(a.id); close() }
	function accept() {
		if (results.length > 0) launch(results[Math.min(current, results.length - 1)])
		else if (query.trim() !== "") { Apps.runCommand(query.trim()); close() }
	}
	readonly property color fgOnSurface: retroLook || xp ? t.windowFg : t.panelFg
	readonly property real fs: t.fontUi.size

	// open: a short fade + rise (skipped with reduce motion)
	ParallelAnimation {
		id: openAnim
		NumberAnimation { target: body; property: "opacity"; from: Chrono.motion ? 0 : 1; to: 1; duration: 160 * Chrono.motion; easing.type: Easing.OutCubic }
		NumberAnimation { target: body; property: "scale"; from: Chrono.motion ? 0.97 : 1; to: 1; duration: 180 * Chrono.motion; easing.type: Easing.OutCubic }
	}

	Item {
		id: body
		anchors.fill: parent
		transformOrigin: Chrono.panelPosition === "top" ? Item.Top : Item.Bottom

		Skin { anchors.fill: parent; kind: "popup"; visible: !launcher.xp }
		Shadow { anchors.fill: parent; visible: launcher.xp; radius: 8; size: 18; strength: 0.5; offsetY: 6 }
		Rectangle {
			visible: launcher.xp
			anchors.fill: parent
			radius: 8
			color: t.windowBg
			border.width: 1
			border.color: t.titleActive
		}
		Rectangle {   // header
			visible: launcher.xp
			anchors { left: parent.left; right: parent.right; top: parent.top }
			height: launcher.headerH
			radius: 8
			gradient: Gradient {
				GradientStop { position: 0; color: Qt.lighter(t.titleActive2, 1.35) }
				GradientStop { position: 0.25; color: t.titleActive2 }
				GradientStop { position: 1; color: t.titleActive }
			}
			Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 8; color: t.titleActive }
			Rectangle { anchors { left: parent.left; right: parent.right; bottom: parent.bottom } height: 2; color: "#e8902c" }
			Row {
				anchors.verticalCenter: parent.verticalCenter
				x: 14
				spacing: 12
				Rectangle {
					width: 40; height: 40; radius: 6
					color: t.accent; border.width: 2; border.color: "white"
					Text { anchors.centerIn: parent; text: (System.userName || "?").charAt(0).toUpperCase(); color: t.accentFg
						font.family: t.fontUi.family; font.pointSize: launcher.fs + 6; font.bold: true }
				}
				Text { anchors.verticalCenter: parent.verticalCenter; text: System.userName; color: t.titleActiveFg
					style: Text.Raised; styleColor: Qt.rgba(0, 0, 0, 0.35)
					font.family: t.fontTitle.family; font.pointSize: launcher.fs + 5; font.bold: true }
			}
		}
		Rectangle {   // footer bar
			visible: launcher.xp
			anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
			height: 52
			radius: 8
			gradient: Gradient {
				GradientStop { position: 0; color: t.titleActive }
				GradientStop { position: 1; color: Qt.darker(t.titleActive, 1.15) }
			}
			Rectangle { anchors { left: parent.left; right: parent.right; top: parent.top } height: 8; color: t.titleActive }
		}

		// ----- retro banner
		Rectangle {
			id: banner
			visible: t.retro && !launcher.xp
			width: visible ? 32 : 0
			// (the 2000s menu has its own header instead)
			anchors { left: parent.left; top: parent.top; bottom: parent.bottom; margins: t.panelStyle === "bevel" ? 3 : 1 }
			gradient: Gradient {
				GradientStop { position: 0; color: t.titleActive2 }
				GradientStop { position: 1; color: t.titleActive }
			}
			Glyph { name: "chrono"; color: t.titleActiveFg; width: 20; height: 20; anchors.horizontalCenter: parent.horizontalCenter; y: 10 }
			Text {
				transformOrigin: Item.Center
				rotation: -90
				x: (parent.width - width) / 2
				y: parent.height - width / 2 - height / 2 - 10
				text: "<b>Chrono</b> " + t.name
				textFormat: Text.StyledText
				color: t.titleActiveFg
				font.family: t.fontTitle.family; font.pointSize: launcher.fs + 5
			}
		}

		Item {
			id: inner
			anchors { left: banner.right; right: parent.right; top: parent.top; bottom: parent.bottom; margins: launcher.pad; topMargin: launcher.pad + launcher.headerH }

			// ----- search
			Item {
				id: search
				width: parent.width; height: 40
				Skin { anchors.fill: parent; kind: "field"; visible: !launcher.xp }
				Rectangle { anchors.fill: parent; visible: launcher.xp; radius: 3; color: "white"; border.color: "#7f9db9" }
				Rectangle {   // focus ring for soft themes
					anchors.fill: parent; anchors.margins: -2
					visible: !launcher.retroLook && field.activeFocus
					radius: Math.max(4, Math.min(10, t.cornerRadius)) + 2
					color: "transparent"; border.width: 2; border.color: Qt.rgba(t.accent.r, t.accent.g, t.accent.b, 0.55)
				}
				Glyph { id: sg; name: "search"; color: launcher.fgOnSurface; width: 16; height: 16; x: 12; anchors.verticalCenter: parent.verticalCenter; opacity: 0.6 }
				TextInput {
					id: field
					anchors { left: sg.right; leftMargin: 10; right: parent.right; rightMargin: 12; verticalCenter: parent.verticalCenter }
					color: launcher.fgOnSurface
					selectionColor: t.selection
					font.family: t.fontUi.family; font.pointSize: launcher.fs + 1
					focus: true
					clip: true
					onTextChanged: launcher.query = text
					Keys.onReturnPressed: launcher.accept()
					Keys.onEnterPressed: launcher.accept()
					Keys.onEscapePressed: launcher.close()
					Keys.onDownPressed: launcher.current = Math.min(launcher.results.length - 1, launcher.current + 1)
					Keys.onUpPressed: launcher.current = Math.max(0, launcher.current - 1)
					Keys.onTabPressed: launcher.current = Math.min(launcher.results.length - 1, launcher.current + 1)
					Text { visible: !field.text; text: qsTr("Search apps, or type a command"); color: field.color; opacity: 0.45; font: field.font }
				}
			}

			// ----- categories
			Column {
				id: cats
				visible: launcher.query === ""
				anchors { top: search.bottom; topMargin: 12; left: parent.left; bottom: footer.top; bottomMargin: 12 }
				width: visible ? 168 : 0
				spacing: 2
				Repeater {
					model: [{ id: "__pinned", name: qsTr("Pinned"), glyph: "pin" }, { id: "", name: qsTr("All apps"), glyph: "apps" }]
						.concat(Apps.categories.map(c => ({ id: c.id, name: c.name, glyph: "" })))
					PanelButton {
						id: catBtn
						required property var modelData
						width: cats.width; height: 32
						padding: 10
						glyph: modelData.glyph
						text: modelData.name
						checked: launcher.category === modelData.id
						fgOverride: launcher.xp ? (checked ? "white" : t.windowFg) : "transparent"
						onClicked: launcher.category = modelData.id
						Binding { target: catBtn.skin; property: "visible"; value: !launcher.xp }
						Rectangle { parent: catBtn; z: -1; visible: launcher.xp && (catBtn.checked || catBtn.hovered); anchors.fill: parent; radius: 3
							color: catBtn.checked ? t.selection : Qt.rgba(t.selection.r, t.selection.g, t.selection.b, 0.15) }
					}
				}
			}

			// ----- results
			Item {
				id: resultsBox
				anchors { top: search.bottom; topMargin: 12; left: cats.right; leftMargin: cats.visible ? 12 : 0; right: parent.right; bottom: footer.top; bottomMargin: 12 }
				Skin { anchors.fill: parent; kind: "well"; visible: t.panelStyle === "bevel" }
				ListView {
					id: list
					anchors.fill: parent
					anchors.margins: t.panelStyle === "bevel" ? 3 : 0
					clip: true
					spacing: 2
					model: launcher.results
					currentIndex: launcher.current
					boundsBehavior: Flickable.StopAtBounds
					highlightMoveDuration: 0
					delegate: Item {
						id: row
						required property var modelData
						required property int index
						readonly property bool pinned: Apps.pinned.indexOf(modelData.id) >= 0
						width: list.width
						height: 48
						Skin { id: rowSkin; anchors.fill: parent; kind: "button"; hovered: ma.containsMouse; pressed: ma.pressed; checked: row.index === launcher.current; visible: !launcher.xp }
						Rectangle { visible: launcher.xp && (row.index === launcher.current || ma.containsMouse); anchors.fill: parent; radius: 3
							color: row.index === launcher.current ? t.selection : Qt.rgba(t.selection.r, t.selection.g, t.selection.b, 0.15) }
						Image {
							id: icon
							x: 10; anchors.verticalCenter: parent.verticalCenter
							width: 32; height: 32
							source: "image://icon/" + encodeURIComponent(row.modelData.icon)
							sourceSize: Qt.size(64, 64)
							asynchronous: true
							smooth: true
							Rectangle {   // pinned badge
								visible: row.pinned
								width: 14; height: 14; radius: launcher.retroLook ? 0 : 7
								anchors { right: parent.right; bottom: parent.bottom; margins: -3 }
								color: t.accent
								Glyph { anchors.centerIn: parent; name: "pin"; width: 10; height: 10; color: t.accentFg }
							}
						}
						readonly property color fg: launcher.xp ? (row.index === launcher.current ? "white" : t.windowFg)
									  : launcher.retroLook ? rowSkin.fg : launcher.fgOnSurface
						Column {
							anchors { left: icon.right; leftMargin: 12; right: parent.right; rightMargin: 10; verticalCenter: parent.verticalCenter }
							spacing: 1
							Text { width: parent.width; text: row.modelData.name; elide: Text.ElideRight; color: row.fg
								font.family: t.fontUi.family; font.pointSize: launcher.fs; font.weight: Font.DemiBold }
							Text { width: parent.width; visible: text !== ""; text: row.modelData.genericName || row.modelData.comment || ""
								elide: Text.ElideRight; color: row.fg; opacity: 0.6
								font.family: t.fontUi.family; font.pointSize: launcher.fs - 1 }
						}
						MouseArea {
							id: ma
							anchors.fill: parent
							hoverEnabled: true
							acceptedButtons: Qt.LeftButton | Qt.RightButton
							onEntered: launcher.current = row.index
							onClicked: (m) => {
								if (m.button === Qt.RightButton) {
									const g = mapToItem(null, m.x, m.y)
									launcher.shell.openAppMenu(launcher.screen, g.x, g.y, row.modelData)
								} else launcher.launch(row.modelData)
							}
						}
					}
					Column {
						anchors.centerIn: parent
						visible: launcher.results.length === 0
						spacing: 8
						Glyph { anchors.horizontalCenter: parent.horizontalCenter; name: launcher.query ? "play" : "pin"; width: 28; height: 28; color: launcher.fgOnSurface; opacity: 0.4 }
						Text {
							anchors.horizontalCenter: parent.horizontalCenter
							width: list.width - 40
							horizontalAlignment: Text.AlignHCenter
							wrapMode: Text.Wrap
							text: launcher.query ? qsTr("No apps found. Press Enter to run “%1”.").arg(launcher.query)
							     : launcher.category === "__pinned" ? qsTr("Nothing pinned yet. Right-click an app and choose “Pin to bar”.") : ""
							color: launcher.fgOnSurface; opacity: 0.6
							font.family: t.fontUi.family; font.pointSize: launcher.fs
						}
					}
				}
			}

			// ----- footer
			Item {
				id: footer
				anchors { left: parent.left; right: parent.right; bottom: parent.bottom }
				height: 36
				Rectangle { visible: !launcher.retroLook && !launcher.xp; anchors { left: parent.left; right: parent.right; top: parent.top; topMargin: -6 } height: 1; color: t.panelBorder }
				Row {
					visible: !launcher.xp
					anchors.verticalCenter: parent.verticalCenter
					spacing: 10
					Rectangle {
						width: 28; height: 28; radius: launcher.retroLook ? 0 : 14
						color: t.accent
						anchors.verticalCenter: parent.verticalCenter
						Text { anchors.centerIn: parent; text: (System.userName || "?").charAt(0).toUpperCase(); color: t.accentFg
							font.family: t.fontUi.family; font.pointSize: launcher.fs; font.bold: true }
					}
					Text { anchors.verticalCenter: parent.verticalCenter; text: System.userName; color: launcher.fgOnSurface
						font.family: t.fontUi.family; font.pointSize: launcher.fs; font.weight: Font.DemiBold }
				}
				Row {
					anchors.right: parent.right
					anchors.verticalCenter: parent.verticalCenter
					spacing: 4
					PanelButton { height: 32; padding: 10; glyph: "settings"; text: qsTr("Settings"); fgOverride: launcher.xp ? t.titleActiveFg : "transparent"; onClicked: { Chrono.action("settings"); launcher.close() } }
					PanelButton { height: 32; width: 32; padding: 8; glyph: "lock"; fgOverride: launcher.xp ? t.titleActiveFg : "transparent"; onClicked: { launcher.close(); Chrono.command("lock") } }
					PanelButton { height: 32; width: 32; padding: 8; glyph: "power"; fgOverride: launcher.xp ? t.titleActiveFg : "transparent"; onClicked: { launcher.close(); launcher.shell.openPowerMenu() } }
				}
			}
		}
	}
}
