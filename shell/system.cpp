#include "system.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSysInfo>
#include <unistd.h>
#include <pwd.h>

static QString readFile(const QString &p)
{
	QFile f(p);
	return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()).trimmed() : QString();
}

System::System(QObject *parent) : QObject(parent)
{
	for (const QString &d : QDir(QStringLiteral("/sys/class/backlight")).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
		m_backlight = QStringLiteral("/sys/class/backlight/") + d;
		break;
	}
	for (const QString &d : QDir(QStringLiteral("/sys/class/power_supply")).entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::System)) {
		const QString p = QStringLiteral("/sys/class/power_supply/") + d;
		if (readFile(p + QStringLiteral("/type")) == QLatin1String("Battery") && QFile::exists(p + QStringLiteral("/capacity"))) {
			m_battery = p;
			break;
		}
	}
	m_fast.setInterval(3000);
	connect(&m_fast, &QTimer::timeout, this, [this] {
		refreshAudio();
		refreshMedia();
	});
	m_fast.start();
	m_slow.setInterval(15000);
	connect(&m_slow, &QTimer::timeout, this, &System::pollSlow);
	m_slow.start();
	refreshAudio();
	refreshBrightness();
	pollSlow();
	refreshMedia();
}

QString System::userName() const
{
	const passwd *pw = getpwuid(getuid());
	if (pw && pw->pw_gecos && *pw->pw_gecos) {
		const QString g = QString::fromLocal8Bit(pw->pw_gecos).section(QLatin1Char(','), 0, 0);
		if (!g.isEmpty())
			return g;
	}
	return pw ? QString::fromLocal8Bit(pw->pw_name) : QString::fromLocal8Bit(qgetenv("USER"));
}

QString System::hostName() const { return QSysInfo::machineHostName(); }

bool System::has(const QString &program) const { return !QStandardPaths::findExecutable(program).isEmpty(); }

void System::run(const QString &prog, const QStringList &args, std::function<void(const QString &)> done)
{
	if (!has(prog)) {
		if (done)
			done(QString());
		return;
	}
	auto *p = new QProcess(this);
	connect(p, &QProcess::finished, this, [p, done](int, QProcess::ExitStatus) {
		if (done)
			done(QString::fromUtf8(p->readAllStandardOutput()));
		p->deleteLater();
	});
	connect(p, &QProcess::errorOccurred, this, [p, done](QProcess::ProcessError e) {
		if (e == QProcess::FailedToStart) {
			if (done)
				done(QString());
			p->deleteLater();
		}
	});
	p->start(prog, args);
}

void System::refreshAudio()
{
	run(QStringLiteral("wpctl"), {QStringLiteral("get-volume"), QStringLiteral("@DEFAULT_AUDIO_SINK@")}, [this](const QString &out) {
		// "Volume: 0.45 [MUTED]"
		static const QRegularExpression re(QStringLiteral("Volume:\\s*([0-9.]+)"));
		const auto m = re.match(out);
		const bool has = m.hasMatch();
		const int v = has ? qRound(m.captured(1).toDouble() * 100) : 0;
		const bool mu = out.contains(QLatin1String("MUTED"));
		if (has != m_hasAudio || v != m_volume || mu != m_muted) {
			m_hasAudio = has;
			m_volume = v;
			m_muted = mu;
			emit audioChanged();
		}
	});
	run(QStringLiteral("wpctl"), {QStringLiteral("get-volume"), QStringLiteral("@DEFAULT_AUDIO_SOURCE@")}, [this](const QString &out) {
		const bool mu = out.contains(QLatin1String("MUTED"));
		if (mu != m_micMuted) {
			m_micMuted = mu;
			emit audioChanged();
		}
	});
}

void System::setVolume(int pct)
{
	pct = qBound(0, pct, 150);
	m_volume = pct;
	emit audioChanged();
	run(QStringLiteral("wpctl"), {QStringLiteral("set-volume"), QStringLiteral("-l"), QStringLiteral("1.5"),
				      QStringLiteral("@DEFAULT_AUDIO_SINK@"), QString::number(pct / 100.0)}, nullptr);
}

void System::toggleMute()
{
	run(QStringLiteral("wpctl"), {QStringLiteral("set-mute"), QStringLiteral("@DEFAULT_AUDIO_SINK@"), QStringLiteral("toggle")},
	    [this](const QString &) { refreshAudio(); });
}

void System::refreshBrightness()
{
	if (m_backlight.isEmpty())
		return;
	const double cur = readFile(m_backlight + QStringLiteral("/brightness")).toDouble();
	const double max = readFile(m_backlight + QStringLiteral("/max_brightness")).toDouble();
	const int pct = max > 0 ? qRound(cur * 100 / max) : 100;
	if (pct != m_brightness) {
		m_brightness = pct;
		emit brightnessChanged();
	}
}

void System::setBrightness(int pct)
{
	pct = qBound(1, pct, 100);
	m_brightness = pct;
	emit brightnessChanged();
	run(QStringLiteral("brightnessctl"), {QStringLiteral("-q"), QStringLiteral("set"), QString::number(pct) + QLatin1Char('%')}, nullptr);
}

void System::pollSlow()
{
	refreshBattery();
	refreshNetwork();
	refreshBrightness();
}

void System::refreshBattery()
{
	if (m_battery.isEmpty())
		return;
	const int pct = readFile(m_battery + QStringLiteral("/capacity")).toInt();
	const QString st = readFile(m_battery + QStringLiteral("/status"));
	const bool ch = st == QLatin1String("Charging") || st == QLatin1String("Full");
	if (pct != m_batteryPct || ch != m_charging) {
		m_batteryPct = pct;
		m_charging = ch;
		emit batteryChanged();
	}
}

void System::refreshNetwork()
{
	QString kind = QStringLiteral("none");
	QString wifiIf;
	for (const QString &i : QDir(QStringLiteral("/sys/class/net")).entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::System)) {
		if (i == QLatin1String("lo"))
			continue;
		const QString p = QStringLiteral("/sys/class/net/") + i;
		if (readFile(p + QStringLiteral("/operstate")) != QLatin1String("up"))
			continue;
		if (QFile::exists(p + QStringLiteral("/wireless"))) {
			kind = QStringLiteral("wifi");
			wifiIf = i;
		} else if (QFile::exists(p + QStringLiteral("/device")) && kind == QLatin1String("none")) {
			kind = QStringLiteral("ethernet");
		}
	}
	int strength = 0;
	if (!wifiIf.isEmpty()) {
		// /proc/net/wireless: "wlan0: 0000   54.  -56.  -256 ..." — link quality out of 70
		const QStringList lines = readFile(QStringLiteral("/proc/net/wireless")).split(QLatin1Char('\n'));
		for (const QString &l : lines)
			if (l.trimmed().startsWith(wifiIf + QLatin1Char(':'))) {
				const QStringList f = l.simplified().split(QLatin1Char(' '));
				if (f.size() > 2)
					strength = qBound(0, int(f[2].toDouble() * 100 / 70), 100);
			}
	}
	const bool changed = kind != m_net || strength != m_wifi;
	m_net = kind;
	m_wifi = strength;
	if (kind == QLatin1String("wifi")) {
		run(QStringLiteral("nmcli"), {QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("active,ssid"),
					      QStringLiteral("dev"), QStringLiteral("wifi")}, [this](const QString &out) {
			QString name;
			for (const QString &l : out.split(QLatin1Char('\n')))
				if (l.startsWith(QLatin1String("yes:")))
					name = l.mid(4);
			if (name != m_netName) {
				m_netName = name;
				emit networkChanged();
			}
		});
	} else if (!m_netName.isEmpty()) {
		m_netName.clear();
		emit networkChanged();
		return;
	}
	if (changed)
		emit networkChanged();
}

QVariantList System::wifiNetworks()
{
	QVariantList out;
	if (!has(QStringLiteral("nmcli")))
		return out;
	QProcess p;
	p.start(QStringLiteral("nmcli"), {QStringLiteral("-t"), QStringLiteral("-f"), QStringLiteral("active,ssid,signal,security"),
					  QStringLiteral("dev"), QStringLiteral("wifi")});
	p.waitForFinished(4000);
	QStringList seen;
	for (const QString &l : QString::fromUtf8(p.readAllStandardOutput()).split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
		const QStringList f = l.split(QLatin1Char(':'));
		if (f.size() < 4 || f[1].isEmpty() || seen.contains(f[1]))
			continue;
		seen << f[1];
		out << QVariantMap{{QStringLiteral("active"), f[0] == QLatin1String("yes")}, {QStringLiteral("ssid"), f[1]},
				   {QStringLiteral("signal"), f[2].toInt()}, {QStringLiteral("secure"), !f[3].isEmpty()}};
	}
	return out;
}

void System::connectWifi(const QString &ssid)
{
	// nmcli asks for a password through the secret agent if one is needed and none is saved
	run(QStringLiteral("nmcli"), {QStringLiteral("--ask"), QStringLiteral("dev"), QStringLiteral("wifi"), QStringLiteral("connect"), ssid},
	    [this](const QString &) { refreshNetwork(); });
}

void System::refreshMedia()
{
	run(QStringLiteral("playerctl"), {QStringLiteral("metadata"), QStringLiteral("--format"),
					  QStringLiteral("{{status}}\t{{title}}\t{{artist}}")}, [this](const QString &out) {
		const QStringList f = out.trimmed().split(QLatin1Char('\t'));
		const bool playing = f.value(0) == QLatin1String("Playing");
		const QString title = f.size() >= 2 ? f[1] : QString();
		const QString artist = f.value(2);
		if (playing != m_mediaPlaying || title != m_mediaTitle || artist != m_mediaArtist) {
			m_mediaPlaying = playing;
			m_mediaTitle = title;
			m_mediaArtist = artist;
			emit mediaChanged();
		}
	});
}

void System::media(const QString &cmd)
{
	run(QStringLiteral("playerctl"), {cmd}, [this](const QString &) { refreshMedia(); });
}

void System::power(const QString &what)
{
	// systemd if it's running, otherwise logind via loginctl (elogind on Void, Gentoo, Artix...)
	const QString tool = QFile::exists(QStringLiteral("/run/systemd/system")) && has(QStringLiteral("systemctl"))
				     ? QStringLiteral("systemctl") : QStringLiteral("loginctl");
	QString verb;
	if (what == QLatin1String("shutdown")) verb = QStringLiteral("poweroff");
	else if (what == QLatin1String("reboot")) verb = QStringLiteral("reboot");
	else if (what == QLatin1String("suspend")) verb = QStringLiteral("suspend");
	else if (what == QLatin1String("hibernate")) verb = QStringLiteral("hibernate");
	if (!verb.isEmpty())
		QProcess::startDetached(tool, {verb});
}
