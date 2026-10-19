#include "backend.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTextStream>
#include <QtGlobal>

static QString readFile(const QString &p)
{
	QFile f(p);
	return f.open(QIODevice::ReadOnly) ? QString::fromUtf8(f.readAll()) : QString();
}

static QString runOut(const QString &prog, const QStringList &args, int ms = 4000)
{
	if (QStandardPaths::findExecutable(prog).isEmpty())
		return {};
	QProcess p;
	p.start(prog, args);
	if (!p.waitForFinished(ms))
		return {};
	return QString::fromUtf8(p.readAllStandardOutput());
}

bool Backend::has(const QString &program) const { return !QStandardPaths::findExecutable(program).isEmpty(); }

void Backend::run(const QString &cmd) const
{
	QProcess::startDetached(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), cmd});
}

// ------------------------------------------------------------------------------ time & date
bool Backend::hasTimedatectl() const
{
	// timedatectl needs systemd-timedated; on elogind systems (Void, Artix...) it isn't there
	return has(QStringLiteral("timedatectl")) && QFile::exists(QStringLiteral("/run/systemd/system"));
}

QStringList Backend::timezones() const
{
	QStringList out;
	if (hasTimedatectl())
		out = runOut(QStringLiteral("timedatectl"), {QStringLiteral("list-timezones")}).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
	if (out.isEmpty()) {
		const QString zi = QStringLiteral("/usr/share/zoneinfo/");
		// zone1970.tab lists the canonical zones
		for (const QString &tab : {QStringLiteral("tzdata.zi"), QStringLiteral("zone1970.tab"), QStringLiteral("zone.tab")}) {
			const QString text = readFile(zi + tab);
			if (text.isEmpty())
				continue;
			for (const QString &line : text.split(QLatin1Char('\n'))) {
				if (tab == QLatin1String("tzdata.zi")) {
					// "Z Europe/Berlin 0:53:28 ..." and "L Europe/Berlin Arctic/Longyearbyen"
					const QStringList f = line.split(QLatin1Char(' '));
					if (f.size() > 1 && f[0] == QLatin1String("Z")) out << f[1];
					else if (f.size() > 2 && f[0] == QLatin1String("L")) out << f[2];
				} else if (!line.startsWith(QLatin1Char('#'))) {
					const QStringList f = line.split(QLatin1Char('\t'));
					if (f.size() > 2) out << f[2];
				}
			}
			if (!out.isEmpty())
				break;
		}
		out << QStringLiteral("UTC");
	}
	out.removeDuplicates();
	out.sort();
	return out;
}

QString Backend::timezone() const
{
	if (hasTimedatectl()) {
		const QString tz = runOut(QStringLiteral("timedatectl"), {QStringLiteral("show"), QStringLiteral("-p"), QStringLiteral("Timezone"), QStringLiteral("--value")}).trimmed();
		if (!tz.isEmpty())
			return tz;
	}
	const QString target = QFileInfo(QStringLiteral("/etc/localtime")).symLinkTarget();
	const int i = target.indexOf(QLatin1String("zoneinfo/"));
	if (i >= 0)
		return target.mid(i + 9);
	const QString tz = readFile(QStringLiteral("/etc/timezone")).trimmed();
	return tz.isEmpty() ? QStringLiteral("UTC") : tz;
}

bool Backend::ntp() const
{
	if (hasTimedatectl())
		return runOut(QStringLiteral("timedatectl"), {QStringLiteral("show"), QStringLiteral("-p"), QStringLiteral("NTP"), QStringLiteral("--value")}).trimmed() == QLatin1String("yes");
	// without systemd: assume on if an NTP daemon is installed and running
	for (const char *d : {"chronyd", "ntpd", "openntpd", "busybox-ntpd"})
		if (!runOut(QStringLiteral("pgrep"), {QStringLiteral("-x"), QLatin1String(d)}).trimmed().isEmpty())
			return true;
	return false;
}

static void finished(QProcess *p, Backend *b)
{
	QObject::connect(p, &QProcess::finished, b, [p, b] {
		emit b->timeChanged();
		p->deleteLater();
	});
}

void Backend::setTimezone(const QString &tz)
{
	if (!QFile::exists(QStringLiteral("/usr/share/zoneinfo/") + tz))
		return;
	auto *p = new QProcess(this);
	finished(p, this);
	if (hasTimedatectl())
		p->start(QStringLiteral("timedatectl"), {QStringLiteral("set-timezone"), tz}); // polkit asks if needed
	else
		p->start(QStringLiteral("pkexec"), {QStringLiteral("ln"), QStringLiteral("-sf"), QStringLiteral("/usr/share/zoneinfo/") + tz, QStringLiteral("/etc/localtime")});
	qputenv("TZ", (QStringLiteral(":") + tz).toUtf8()); // our own clock follows right away
}

void Backend::setNtp(bool on)
{
	if (!hasTimedatectl())
		return;
	auto *p = new QProcess(this);
	finished(p, this);
	p->start(QStringLiteral("timedatectl"), {QStringLiteral("set-ntp"), on ? QStringLiteral("true") : QStringLiteral("false")});
}

void Backend::setDateTime(const QString &when)
{
	auto *p = new QProcess(this);
	finished(p, this);
	if (hasTimedatectl())
		p->start(QStringLiteral("timedatectl"), {QStringLiteral("set-time"), when});
	else
		p->start(QStringLiteral("pkexec"), {QStringLiteral("date"), QStringLiteral("-s"), when});
}

// ------------------------------------------------------------------------------ keyboard
static QString xkbList()
{
	for (const char *p : {"/usr/share/X11/xkb/rules/evdev.lst", "/usr/share/X11/xkb/rules/base.lst"}) {
		const QString t = readFile(QLatin1String(p));
		if (!t.isEmpty())
			return t;
	}
	return {};
}

static QList<QStringList> xkbSection(const QString &name)
{
	QList<QStringList> out;
	bool in = false;
	for (const QString &line : xkbList().split(QLatin1Char('\n'))) {
		if (line.startsWith(QLatin1Char('!'))) {
			in = line.mid(1).trimmed() == name;
			continue;
		}
		if (!in || line.trimmed().isEmpty())
			continue;
		const QString l = line.trimmed();
		const int sp = l.indexOf(QLatin1Char(' '));
		if (sp < 0)
			continue;
		out << QStringList{l.left(sp), l.mid(sp).trimmed()};
	}
	return out;
}

QVariantList Backend::keyboardLayouts() const
{
	QVariantList out;
	for (const QStringList &e : xkbSection(QStringLiteral("layout")))
		out << QVariantMap{{QStringLiteral("id"), e[0]}, {QStringLiteral("name"), e[1]}};
	return out;
}

QVariantList Backend::keyboardVariants(const QString &layout) const
{
	QVariantList out{QVariantMap{{QStringLiteral("id"), QString()}, {QStringLiteral("name"), tr("Default")}}};
	for (const QStringList &e : xkbSection(QStringLiteral("variant"))) {
		// "intl            us: English (US, intl., with dead keys)"
		const int c = e[1].indexOf(QLatin1Char(':'));
		if (c > 0 && e[1].left(c) == layout)
			out << QVariantMap{{QStringLiteral("id"), e[0]}, {QStringLiteral("name"), e[1].mid(c + 1).trimmed()}};
	}
	return out;
}

QVariantList Backend::keyboardModels() const
{
	QVariantList out;
	for (const QStringList &e : xkbSection(QStringLiteral("model")))
		out << QVariantMap{{QStringLiteral("id"), e[0]}, {QStringLiteral("name"), e[1]}};
	return out;
}

// ------------------------------------------------------------------------------ appearance
static QStringList iconDirs()
{
	QStringList d{QDir::homePath() + QStringLiteral("/.icons"), QDir::homePath() + QStringLiteral("/.local/share/icons")};
	QString sys = QString::fromLocal8Bit(qgetenv("XDG_DATA_DIRS"));
	if (sys.isEmpty())
		sys = QStringLiteral("/usr/local/share:/usr/share");
	for (const QString &s : sys.split(QLatin1Char(':'), Qt::SkipEmptyParts))
		d << s + QStringLiteral("/icons");
	return d;
}

QStringList Backend::cursorThemes() const
{
	QStringList out;
	for (const QString &d : iconDirs())
		for (const QString &t : QDir(d).entryList(QDir::Dirs | QDir::NoDotAndDotDot))
			if (QDir(d + QLatin1Char('/') + t + QStringLiteral("/cursors")).exists())
				out << t;
	out.removeDuplicates();
	out.sort(Qt::CaseInsensitive);
	return out;
}

QStringList Backend::iconThemes() const
{
	QStringList out;
	for (const QString &d : iconDirs())
		for (const QString &t : QDir(d).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
			const QString idx = readFile(d + QLatin1Char('/') + t + QStringLiteral("/index.theme"));
			if (idx.contains(QLatin1String("Directories=")) && t != QLatin1String("hicolor") && t != QLatin1String("default"))
				out << t;
		}
	out.removeDuplicates();
	out.sort(Qt::CaseInsensitive);
	return out;
}

// ------------------------------------------------------------------------------ sound
static QVariantList pactlList(const QString &what, const QString &defName)
{
	QVariantList out;
	const QJsonDocument d = QJsonDocument::fromJson(runOut(QStringLiteral("pactl"), {QStringLiteral("-f"), QStringLiteral("json"), QStringLiteral("list"), what}).toUtf8());
	for (const QJsonValue &v : d.array()) {
		const QJsonObject o = v.toObject();
		const QString name = o.value(QStringLiteral("name")).toString();
		if (name.endsWith(QLatin1String(".monitor")))
			continue;
		out << QVariantMap{{QStringLiteral("name"), name},
				   {QStringLiteral("description"), o.value(QStringLiteral("description")).toString()},
				   {QStringLiteral("default"), name == defName}};
	}
	return out;
}

QVariantList Backend::sinks() const
{
	return pactlList(QStringLiteral("sinks"), runOut(QStringLiteral("pactl"), {QStringLiteral("get-default-sink")}).trimmed());
}

QVariantList Backend::sources() const
{
	return pactlList(QStringLiteral("sources"), runOut(QStringLiteral("pactl"), {QStringLiteral("get-default-source")}).trimmed());
}

void Backend::setDefaultSink(const QString &name)
{
	runOut(QStringLiteral("pactl"), {QStringLiteral("set-default-sink"), name});
	emit soundChanged();
}

void Backend::setDefaultSource(const QString &name)
{
	runOut(QStringLiteral("pactl"), {QStringLiteral("set-default-source"), name});
	emit soundChanged();
}

// ------------------------------------------------------------------------------ autostart
static QString userAutostart()
{
	return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/autostart");
}

QVariantList Backend::autostart() const
{
	QVariantList out;
	QStringList seen;
	for (const QString &dir : {userAutostart(), QStringLiteral("/etc/xdg/autostart")}) {
		for (const QFileInfo &fi : QDir(dir).entryInfoList({QStringLiteral("*.desktop")}, QDir::Files, QDir::Name)) {
			if (seen.contains(fi.fileName()))
				continue;
			seen << fi.fileName();
			QString name = fi.completeBaseName(), only;
			bool hidden = false;
			for (const QString &l : readFile(fi.filePath()).split(QLatin1Char('\n'))) {
				if (l.startsWith(QLatin1String("Name="))) name = l.mid(5);
				else if (l.startsWith(QLatin1String("Hidden="))) hidden = l.mid(7).trimmed() == QLatin1String("true");
				else if (l.startsWith(QLatin1String("OnlyShowIn="))) only = l.mid(11);
			}
			if (!only.isEmpty() && !only.contains(QLatin1String("Chrono"), Qt::CaseInsensitive))
				continue;
			out << QVariantMap{{QStringLiteral("file"), fi.fileName()}, {QStringLiteral("name"), name},
					   {QStringLiteral("enabled"), !hidden}, {QStringLiteral("user"), dir == userAutostart()}};
		}
	}
	return out;
}

void Backend::setAutostart(const QString &file, bool enabled)
{
	// a copy in ~/.config/autostart with Hidden=true/false overrides the system entry
	QDir().mkpath(userAutostart());
	const QString user = userAutostart() + QLatin1Char('/') + file;
	QString text = readFile(user);
	if (text.isEmpty())
		text = readFile(QStringLiteral("/etc/xdg/autostart/") + file);
	if (text.isEmpty())
		return;
	QStringList lines = text.split(QLatin1Char('\n'));
	lines.erase(std::remove_if(lines.begin(), lines.end(), [](const QString &l) { return l.startsWith(QLatin1String("Hidden=")); }), lines.end());
	const int de = lines.indexOf(QStringLiteral("[Desktop Entry]"));
	lines.insert(de >= 0 ? de + 1 : 0, QStringLiteral("Hidden=") + (enabled ? QStringLiteral("false") : QStringLiteral("true")));
	QFile f(user);
	if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
		f.write(lines.join(QLatin1Char('\n')).toUtf8());
}

// ------------------------------------------------------------------------------ about
QVariantMap Backend::systemInfo() const
{
	QVariantMap m;
	QString os;
	for (const QString &l : readFile(QStringLiteral("/etc/os-release")).split(QLatin1Char('\n')))
		if (l.startsWith(QLatin1String("PRETTY_NAME=")))
			os = l.mid(12).remove(QLatin1Char('"'));
	m[QStringLiteral("os")] = os.isEmpty() ? QSysInfo::prettyProductName() : os;
	m[QStringLiteral("kernel")] = QSysInfo::kernelVersion();
	m[QStringLiteral("host")] = QSysInfo::machineHostName();
	m[QStringLiteral("arch")] = QSysInfo::currentCpuArchitecture();
	QString cpu;
	int cores = 0;
	for (const QString &l : readFile(QStringLiteral("/proc/cpuinfo")).split(QLatin1Char('\n'))) {
		if (l.startsWith(QLatin1String("model name")) && cpu.isEmpty())
			cpu = l.section(QLatin1Char(':'), 1).trimmed();
		if (l.startsWith(QLatin1String("processor")))
			cores++;
	}
	m[QStringLiteral("cpu")] = cores ? QStringLiteral("%1 × %2").arg(cores).arg(cpu) : cpu;
	for (const QString &l : readFile(QStringLiteral("/proc/meminfo")).split(QLatin1Char('\n')))
		if (l.startsWith(QLatin1String("MemTotal:"))) {
			const double kb = l.section(QLatin1Char(':'), 1).trimmed().section(QLatin1Char(' '), 0, 0).toDouble();
			m[QStringLiteral("memory")] = QString::number(kb / 1024 / 1024, 'f', 1) + QStringLiteral(" GiB");
		}
	QStringList gpus;
	for (const QString &l : runOut(QStringLiteral("lspci"), {}).split(QLatin1Char('\n')))
		if (l.contains(QLatin1String("VGA compatible")) || l.contains(QLatin1String("3D controller")) || l.contains(QLatin1String("Display controller")))
			gpus << l.section(QLatin1Char(':'), 2).trimmed();
	m[QStringLiteral("gpu")] = gpus.join(QStringLiteral(", "));
	m[QStringLiteral("qt")] = QString::fromLatin1(qVersion());
	m[QStringLiteral("session")] = QString::fromLocal8Bit(qgetenv("XDG_SESSION_TYPE"));
	return m;
}
