#include "apps.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QLocale>
#include <QProcess>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>
#include <algorithm>

extern "C" {
#include "ini.h"
}

static QString prefsPath()
{
	return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/chrono/shell.conf");
}

static QStringList appDirs()
{
	QStringList dirs;
	QString home = QString::fromLocal8Bit(qgetenv("XDG_DATA_HOME"));
	if (home.isEmpty())
		home = QDir::homePath() + QStringLiteral("/.local/share");
	dirs << home + QStringLiteral("/applications");
	QString sys = QString::fromLocal8Bit(qgetenv("XDG_DATA_DIRS"));
	if (sys.isEmpty())
		sys = QStringLiteral("/usr/local/share:/usr/share");
	for (const QString &d : sys.split(QLatin1Char(':'), Qt::SkipEmptyParts))
		dirs << d + QStringLiteral("/applications");
	dirs << QStringLiteral("/var/lib/flatpak/exports/share/applications")
	     << QDir::homePath() + QStringLiteral("/.local/share/flatpak/exports/share/applications");
	dirs.removeDuplicates();
	return dirs;
}

// Minimal .desktop parser: returns "Section/Key" -> value, preferring the current locale's translations.
static QHash<QString, QString> parseDesktop(const QString &path)
{
	QHash<QString, QString> out;
	QFile f(path);
	if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
		return out;
	const QString lang = QLocale().name();             // de_DE
	const QString lang2 = lang.section(QLatin1Char('_'), 0, 0); // de
	QHash<QString, int> prio;
	QString section;
	QTextStream ts(&f);
	while (!ts.atEnd()) {
		const QString line = ts.readLine().trimmed();
		if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
			continue;
		if (line.startsWith(QLatin1Char('['))) {
			section = line.mid(1, line.size() - 2);
			continue;
		}
		const int eq = line.indexOf(QLatin1Char('='));
		if (eq <= 0)
			continue;
		QString key = line.left(eq).trimmed();
		const QString val = line.mid(eq + 1).trimmed();
		int p = 1;
		const int br = key.indexOf(QLatin1Char('['));
		if (br > 0) {
			const QString loc = key.mid(br + 1, key.size() - br - 2);
			key = key.left(br);
			if (loc == lang) p = 3;
			else if (loc == lang2) p = 2;
			else continue;
		}
		const QString k = section + QLatin1Char('/') + key;
		if (p >= prio.value(k, 0)) {
			out[k] = val;
			prio[k] = p;
		}
	}
	return out;
}

static QString cleanExec(QString exec, const QString &name, const QString &icon)
{
	QString out;
	for (int i = 0; i < exec.size(); i++) {
		if (exec[i] == QLatin1Char('%') && i + 1 < exec.size()) {
			const QChar c = exec[++i];
			if (c == QLatin1Char('%')) out += QLatin1Char('%');
			else if (c == QLatin1Char('c')) out += QLatin1Char('"') + name + QLatin1Char('"');
			else if (c == QLatin1Char('i') && !icon.isEmpty()) out += QStringLiteral("--icon ") + icon;
			continue;
		}
		out += exec[i];
	}
	return out.simplified();
}

Apps::Apps(QObject *parent) : QObject(parent)
{
	m_rescan.setSingleShot(true);
	m_rescan.setInterval(1500);
	connect(&m_rescan, &QTimer::timeout, this, &Apps::scan);
	connect(&m_watch, &QFileSystemWatcher::directoryChanged, this, [this] { m_rescan.start(); });
	loadPrefs();
	scan();
}

void Apps::scan()
{
	QVariantList list;
	QHash<QString, int> index;
	for (const QString &dir : appDirs()) {
		if (!QDir(dir).exists())
			continue;
		if (!m_watch.directories().contains(dir))
			m_watch.addPath(dir);
		QDirIterator it(dir, {QStringLiteral("*.desktop")}, QDir::Files, QDirIterator::Subdirectories);
		while (it.hasNext()) {
			const QString path = it.next();
			QString id = path.mid(dir.size() + 1);
			id.replace(QLatin1Char('/'), QLatin1Char('-'));
			if (index.contains(id))
				continue; // earlier dirs (user) override later ones
			const auto d = parseDesktop(path);
			const QString e = QStringLiteral("Desktop Entry/");
			if (d.value(e + QStringLiteral("Type")) != QLatin1String("Application"))
				continue;
			const bool hidden = d.value(e + QStringLiteral("NoDisplay")) == QLatin1String("true") ||
					    d.value(e + QStringLiteral("Hidden")) == QLatin1String("true");
			const QString only = d.value(e + QStringLiteral("OnlyShowIn"));
			const QString notIn = d.value(e + QStringLiteral("NotShowIn"));
			bool shown = !hidden && (only.isEmpty() || only.contains(QLatin1String("Chrono"), Qt::CaseInsensitive)) &&
				     !notIn.contains(QLatin1String("Chrono"), Qt::CaseInsensitive);
			const QString tryExec = d.value(e + QStringLiteral("TryExec"));
			if (!tryExec.isEmpty() && QStandardPaths::findExecutable(tryExec).isEmpty() && !QFile::exists(tryExec))
				shown = false;
			const QString name = d.value(e + QStringLiteral("Name"));
			const QString icon = d.value(e + QStringLiteral("Icon"));
			QVariantList actions;
			for (const QString &a : d.value(e + QStringLiteral("Actions")).split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
				const QString s = QStringLiteral("Desktop Action ") + a + QLatin1Char('/');
				if (d.contains(s + QStringLiteral("Exec")))
					actions << QVariantMap{{QStringLiteral("id"), a}, {QStringLiteral("name"), d.value(s + QStringLiteral("Name"))}};
			}
			QVariantMap m{
				{QStringLiteral("id"), id},
				{QStringLiteral("path"), path},
				{QStringLiteral("name"), name},
				{QStringLiteral("genericName"), d.value(e + QStringLiteral("GenericName"))},
				{QStringLiteral("comment"), d.value(e + QStringLiteral("Comment"))},
				{QStringLiteral("icon"), icon.isEmpty() ? QStringLiteral("application-x-executable") : icon},
				{QStringLiteral("exec"), cleanExec(d.value(e + QStringLiteral("Exec")), name, icon)},
				{QStringLiteral("terminal"), d.value(e + QStringLiteral("Terminal")) == QLatin1String("true")},
				{QStringLiteral("categories"), d.value(e + QStringLiteral("Categories")).split(QLatin1Char(';'), Qt::SkipEmptyParts)},
				{QStringLiteral("keywords"), d.value(e + QStringLiteral("Keywords"))},
				{QStringLiteral("wmClass"), d.value(e + QStringLiteral("StartupWMClass"))},
				{QStringLiteral("shown"), shown},
				{QStringLiteral("actions"), actions},
			};
			if (name.isEmpty())
				continue;
			index.insert(id, list.size());
			list << m;
		}
	}
	std::sort(list.begin(), list.end(), [](const QVariant &a, const QVariant &b) {
		return a.toMap().value(QStringLiteral("name")).toString().compare(
			       b.toMap().value(QStringLiteral("name")).toString(), Qt::CaseInsensitive) < 0;
	});
	index.clear();
	for (int i = 0; i < list.size(); i++)
		index.insert(list[i].toMap().value(QStringLiteral("id")).toString(), i);
	m_list = list;
	m_index = index;

	if (m_pinned.isEmpty() && !QSettings(prefsPath(), QSettings::IniFormat).contains(QStringLiteral("dock/pinned"))) {
		// first run: pin a terminal, a file manager, a browser and Settings
		for (const char *cat : {"TerminalEmulator", "FileManager", "WebBrowser"})
			for (const QVariant &v : std::as_const(m_list)) {
				const QVariantMap a = v.toMap();
				if (a.value(QStringLiteral("shown")).toBool() &&
				    a.value(QStringLiteral("categories")).toStringList().contains(QLatin1String(cat))) {
					m_pinned << a.value(QStringLiteral("id")).toString();
					break;
				}
			}
		if (m_index.contains(QStringLiteral("chrono-settings.desktop")))
			m_pinned << QStringLiteral("chrono-settings.desktop");
		emit pinnedChanged();
	}
	emit changed();
}

QVariantList Apps::categories() const
{
	static const QList<QPair<QString, QString>> cats = {
		{QStringLiteral("AudioVideo"), QStringLiteral("Multimedia")}, {QStringLiteral("Development"), QStringLiteral("Development")},
		{QStringLiteral("Education"), QStringLiteral("Education")}, {QStringLiteral("Game"), QStringLiteral("Games")},
		{QStringLiteral("Graphics"), QStringLiteral("Graphics")}, {QStringLiteral("Network"), QStringLiteral("Internet")},
		{QStringLiteral("Office"), QStringLiteral("Office")}, {QStringLiteral("Science"), QStringLiteral("Science")},
		{QStringLiteral("Settings"), QStringLiteral("Settings")}, {QStringLiteral("System"), QStringLiteral("System")},
		{QStringLiteral("Utility"), QStringLiteral("Utilities")}};
	QVariantList out;
	for (const auto &c : cats)
		if (!inCategory(c.first).isEmpty())
			out << QVariantMap{{QStringLiteral("id"), c.first}, {QStringLiteral("name"), c.second}};
	return out;
}

QVariantList Apps::inCategory(const QString &cat) const
{
	QVariantList out;
	for (const QVariant &v : m_list) {
		const QVariantMap a = v.toMap();
		if (a.value(QStringLiteral("shown")).toBool() && (cat.isEmpty() || a.value(QStringLiteral("categories")).toStringList().contains(cat)))
			out << a;
	}
	return out;
}

QVariantList Apps::search(const QString &text, int limit) const
{
	const QString q = text.trimmed().toLower();
	QList<QPair<int, QVariantMap>> scored;
	for (const QVariant &v : m_list) {
		const QVariantMap a = v.toMap();
		if (!a.value(QStringLiteral("shown")).toBool())
			continue;
		const QString name = a.value(QStringLiteral("name")).toString().toLower();
		int score = 0;
		if (q.isEmpty())
			score = 1;
		else if (name.startsWith(q))
			score = 100;
		else if (name.contains(QLatin1Char(' ') + q))
			score = 80;
		else if (name.contains(q))
			score = 60;
		else if (a.value(QStringLiteral("genericName")).toString().toLower().contains(q))
			score = 40;
		else if (a.value(QStringLiteral("keywords")).toString().toLower().contains(q))
			score = 30;
		else if (a.value(QStringLiteral("exec")).toString().toLower().section(QLatin1Char(' '), 0, 0).contains(q))
			score = 25;
		else if (a.value(QStringLiteral("comment")).toString().toLower().contains(q))
			score = 10;
		if (!score)
			continue;
		score += qMin(m_usage.value(a.value(QStringLiteral("id")).toString()), 30);
		scored.append({score, a});
	}
	std::stable_sort(scored.begin(), scored.end(), [](const auto &x, const auto &y) { return x.first > y.first; });
	QVariantList out;
	for (const auto &s : scored) {
		if (out.size() >= limit)
			break;
		out << s.second;
	}
	return out;
}

QVariantMap Apps::app(const QString &id) const
{
	const int i = m_index.value(id, -1);
	return i >= 0 ? m_list[i].toMap() : QVariantMap();
}

QVariantMap Apps::forAppId(const QString &appId) const
{
	if (appId.isEmpty())
		return {};
	const QString lower = appId.toLower();
	if (m_index.contains(appId + QStringLiteral(".desktop")))
		return app(appId + QStringLiteral(".desktop"));
	for (const QVariant &v : m_list) {
		const QVariantMap a = v.toMap();
		const QString id = a.value(QStringLiteral("id")).toString().toLower();
		if (id == lower + QStringLiteral(".desktop") || a.value(QStringLiteral("wmClass")).toString().toLower() == lower)
			return a;
	}
	for (const QVariant &v : m_list) {
		const QVariantMap a = v.toMap();
		const QString id = a.value(QStringLiteral("id")).toString().toLower();
		if (id.endsWith(QLatin1Char('.') + lower + QStringLiteral(".desktop")) ||
		    a.value(QStringLiteral("exec")).toString().section(QLatin1Char(' '), 0, 0).section(QLatin1Char('/'), -1).toLower() == lower)
			return a;
	}
	return {};
}

static QString terminalCommand()
{
	chrono_ini ini = {};
	QString cfg = QString::fromLocal8Bit(qgetenv("CHRONO_CONFIG"));
	if (cfg.isEmpty())
		cfg = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/chrono/chrono.conf");
	const QByteArray p = cfg.toLocal8Bit();
	chrono_ini_load(&ini, p.constData());
	QString t = QString::fromUtf8(chrono_ini_get(&ini, "general.terminal", ""));
	chrono_ini_free(&ini);
	if (t.isEmpty())
		for (const char *c : {"interstellar", "kitty", "foot", "alacritty", "wezterm", "ghostty", "konsole", "gnome-terminal", "xterm"})
			if (!QStandardPaths::findExecutable(QLatin1String(c)).isEmpty()) {
				t = QLatin1String(c);
				break;
			}
	return t.isEmpty() ? QStringLiteral("xterm") : t;
}

void Apps::runCommand(const QString &cmd)
{
	QProcess::startDetached(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), QStringLiteral("exec ") + cmd});
}

void Apps::launch(const QString &id, const QString &action)
{
	const QVariantMap a = app(id);
	if (a.isEmpty())
		return;
	QString exec = a.value(QStringLiteral("exec")).toString();
	if (!action.isEmpty()) {
		const auto d = parseDesktop(a.value(QStringLiteral("path")).toString());
		exec = cleanExec(d.value(QStringLiteral("Desktop Action ") + action + QStringLiteral("/Exec")),
				 a.value(QStringLiteral("name")).toString(), a.value(QStringLiteral("icon")).toString());
	}
	if (exec.isEmpty())
		return;
	if (a.value(QStringLiteral("terminal")).toBool())
		exec = terminalCommand() + QStringLiteral(" -e ") + exec;
	runCommand(exec);
	m_usage[id]++;
	savePrefs();
}

void Apps::pin(const QString &id)
{
	if (m_pinned.contains(id) || id.isEmpty())
		return;
	m_pinned << id;
	savePrefs();
	emit pinnedChanged();
}

void Apps::unpin(const QString &id)
{
	if (m_pinned.removeAll(id)) {
		savePrefs();
		emit pinnedChanged();
	}
}

void Apps::movePinned(int from, int to)
{
	if (from < 0 || from >= m_pinned.size() || to < 0 || to >= m_pinned.size() || from == to)
		return;
	m_pinned.move(from, to);
	savePrefs();
	emit pinnedChanged();
}

void Apps::loadPrefs()
{
	QSettings s(prefsPath(), QSettings::IniFormat);
	m_pinned = s.value(QStringLiteral("dock/pinned")).toStringList();
	s.beginGroup(QStringLiteral("usage"));
	for (const QString &k : s.childKeys())
		m_usage[QString(k).replace(QLatin1Char('|'), QLatin1Char('/'))] = s.value(k).toInt();
	s.endGroup();
}

void Apps::savePrefs()
{
	QSettings s(prefsPath(), QSettings::IniFormat);
	s.setValue(QStringLiteral("dock/pinned"), m_pinned);
	s.beginGroup(QStringLiteral("usage"));
	for (auto it = m_usage.cbegin(); it != m_usage.cend(); ++it)
		s.setValue(QString(it.key()).replace(QLatin1Char('/'), QLatin1Char('|')), it.value());
	s.endGroup();
}
