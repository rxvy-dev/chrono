// Time & Date: time zone, automatic time, setting the clock by hand, and how the panel clock looks.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

SettingsPage {
	id: page
	title: qsTr("Time & Date")
	property string tz: Backend.timezone()
	property bool ntp: Backend.ntp()
	property date now: new Date()
	property var zones: []
	Component.onCompleted: zones = Backend.timezones()
	Timer { interval: 1000; running: true; repeat: true; onTriggered: page.now = new Date() }
	Connections { target: Backend; function onTimeChanged() { page.tz = Backend.timezone(); page.ntp = Backend.ntp() } }

	// big clock
	ColumnLayout {
		spacing: 0
		Label { text: Qt.formatTime(page.now, Chrono.config["clock.format"] === "24h" ? "HH:mm:ss" : Chrono.config["clock.format"] === "12h" ? "h:mm:ss AP" : Qt.locale().timeFormat(Locale.LongFormat).replace(/ t+$/, "").replace(" t", ""))
			font.pointSize: 32; font.bold: true }
		Label { text: Qt.formatDate(page.now, Locale.LongFormat) + "  ·  " + page.tz; opacity: 0.7 }
	}

	Section {
		title: qsTr("Time zone")
		SettingRow {
			label: qsTr("Time zone")
			description: Backend.hasTimedatectl ? "" : qsTr("You'll be asked for your password (pkexec).")
			ColumnLayout {
				width: 320
				TextField { id: tzFilter; Layout.fillWidth: true; placeholderText: qsTr("Search, e.g. Vancouver or Berlin") }
				ComboBox {
					id: tzBox
					Layout.fillWidth: true
					readonly property var list: page.zones.filter(z => tzFilter.text === "" || z.toLowerCase().indexOf(tzFilter.text.toLowerCase().replace(/ /g, "_")) >= 0)
					model: list
					currentIndex: list.indexOf(page.tz)
					onActivated: (i) => Backend.setTimezone(list[i])
				}
			}
		}
		Button {
			text: qsTr("Detect from my internet connection (asks ipapi.co)")
			visible: Backend.has("curl")
			onClicked: Backend.run("tz=$(curl -fsS --max-time 5 https://ipapi.co/timezone) && [ -n \"$tz\" ] && " +
				(Backend.hasTimedatectl ? "timedatectl set-timezone \"$tz\"" : "pkexec ln -sf /usr/share/zoneinfo/$tz /etc/localtime"))
		}
	}

	Section {
		title: qsTr("Date & time")
		SettingRow {
			label: qsTr("Set the time automatically")
			description: Backend.hasTimedatectl ? qsTr("Uses network time (NTP).")
				: qsTr("Install and enable an NTP service (chrony, openntpd or ntpd) to keep the clock right; it's detected here.")
			Switch { checked: page.ntp; enabled: Backend.hasTimedatectl; onToggled: Backend.setNtp(checked) }
		}
		SettingRow {
			label: qsTr("Date and time")
			enabled: !page.ntp
			Row {
				spacing: 6
				TextField { id: dateField; width: 120; text: Qt.formatDate(page.now, "yyyy-MM-dd"); inputMask: "9999-99-99"; enabled: !page.ntp }
				TextField { id: timeField; width: 90; text: Qt.formatTime(page.now, "HH:mm"); inputMask: "99:99"; enabled: !page.ntp }
				Button { text: qsTr("Set"); enabled: !page.ntp; onClicked: Backend.setDateTime(dateField.text + " " + timeField.text + ":00") }
			}
		}
	}

	Section {
		title: qsTr("Panel clock")
		ConfigCombo { label: qsTr("Time format"); section: "clock"; key: "format"; defaultValue: "auto"
			options: [{ value: "auto", text: qsTr("Like my language (%1)").arg(Qt.formatTime(new Date(2026, 0, 1, 15, 30), Qt.locale().timeFormat(Locale.ShortFormat))) },
				  { value: "12h", text: qsTr("12-hour (3:30 PM)") }, { value: "24h", text: qsTr("24-hour (15:30)") }] }
		ConfigSwitch { label: qsTr("Show seconds"); section: "clock"; key: "seconds"; defaultValue: false }
		ConfigSwitch { label: qsTr("Show the date"); description: qsTr("Retro themes hide it unless this is on."); section: "clock"; key: "date"; defaultValue: !Chrono.theme.retro }
		ConfigCombo { label: qsTr("Date style"); section: "clock"; key: "date_format"; defaultValue: "ddd d"
			options: ["ddd d", "ddd d MMM", "d MMM", "dd/MM", "MM/dd", "yyyy-MM-dd", "dddd d MMMM"].map(f => ({ value: f, text: Qt.formatDate(page.now, f) })) }
	}
	Item { Layout.preferredHeight: 20 }
}
