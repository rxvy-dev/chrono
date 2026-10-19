import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

SettingsPage {
	id: page
	title: qsTr("Keyboard")
	readonly property var layouts: Backend.keyboardLayouts()
	readonly property var current: (Chrono.config["keyboard.layout"] || "us").split(",").filter(s => s !== "")
	readonly property var variants: (Chrono.config["keyboard.variant"] || "").split(",")
	function save(list, vars) {
		Chrono.setConfig("keyboard", "layout", list.join(","))
		Chrono.setConfig("keyboard", "variant", vars.join(","))
	}

	Section {
		title: qsTr("Layouts")
		Label { text: qsTr("With more than one layout, switch with the shortcut below."); opacity: 0.7 }
		Repeater {
			model: page.current
			RowLayout {
				required property string modelData
				required property int index
				Layout.fillWidth: true
				Label { text: (page.layouts.find(l => l.id === modelData) || { name: modelData }).name; Layout.fillWidth: true }
				ComboBox {
					Layout.preferredWidth: 280
					readonly property var vs: Backend.keyboardVariants(modelData)
					model: vs.map(v => v.name)
					currentIndex: Math.max(0, vs.findIndex(v => v.id === (page.variants[index] || "")))
					onActivated: (i) => { const v = page.current.map((_, j) => page.variants[j] || ""); v[index] = vs[i].id; page.save(page.current, v) }
				}
				ToolButton { text: "↑"; enabled: index > 0; onClicked: { const l = page.current.slice(), v = page.current.map((_, j) => page.variants[j] || "")
					;[l[index - 1], l[index]] = [l[index], l[index - 1]]; [v[index - 1], v[index]] = [v[index], v[index - 1]]; page.save(l, v) } }
				ToolButton { text: qsTr("Remove"); enabled: page.current.length > 1; onClicked: { const l = page.current.slice(), v = page.current.map((_, j) => page.variants[j] || "")
					l.splice(index, 1); v.splice(index, 1); page.save(l, v) } }
			}
		}
		RowLayout {
			TextField { id: lf; Layout.preferredWidth: 200; placeholderText: qsTr("Search layouts") }
			ComboBox {
				id: addBox
				Layout.preferredWidth: 300
				readonly property var list: page.layouts.filter(l => page.current.indexOf(l.id) < 0 && (lf.text === "" || l.name.toLowerCase().indexOf(lf.text.toLowerCase()) >= 0))
				model: list.map(l => l.name)
			}
			Button { text: qsTr("Add"); enabled: addBox.list.length > 0; onClicked: { const l = page.current.concat([addBox.list[addBox.currentIndex].id]); page.save(l, l.map((_, j) => page.variants[j] || "")) } }
		}
		ConfigCombo { label: qsTr("Switch layouts with"); section: "keyboard"; key: "options"; defaultValue: ""
			options: [{ value: "", text: qsTr("Nothing") }, { value: "grp:alt_shift_toggle", text: "Alt+Shift" }, { value: "grp:win_space_toggle", text: "Super+Space" },
				  { value: "grp:ctrl_shift_toggle", text: "Ctrl+Shift" }, { value: "grp:caps_toggle", text: "Caps Lock" }] }
		ConfigCombo { label: qsTr("Caps Lock"); section: "keyboard"; key: "caps"; defaultValue: ""
			description: qsTr("Applied together with the switch shortcut.")
			options: [{ value: "", text: qsTr("Caps Lock") }, { value: "caps:escape", text: qsTr("Escape") }, { value: "ctrl:nocaps", text: qsTr("Ctrl") }, { value: "caps:none", text: qsTr("Disabled") }] }
		ConfigCombo { label: qsTr("Keyboard model"); section: "keyboard"; key: "model"; defaultValue: ""
			options: [{ value: "", text: qsTr("Automatic") }].concat(Backend.keyboardModels().map(m => ({ value: m.id, text: m.name }))) ; comboWidth: 320 }
	}
	Section {
		title: qsTr("Typing")
		ConfigSpin { label: qsTr("Repeat delay"); section: "keyboard"; key: "repeat_delay"; defaultValue: 300; from: 100; to: 1500; step: 25; suffix: " ms" }
		ConfigSpin { label: qsTr("Repeat rate"); section: "keyboard"; key: "repeat_rate"; defaultValue: 30; from: 1; to: 100; suffix: qsTr(" /s") }
		ConfigSwitch { label: qsTr("Num Lock on at start"); section: "keyboard"; key: "numlock"; defaultValue: true }
		SettingRow { label: qsTr("Try it"); TextField { width: 260; placeholderText: qsTr("Type here to test") } }
	}
	Item { Layout.preferredHeight: 20 }
}
