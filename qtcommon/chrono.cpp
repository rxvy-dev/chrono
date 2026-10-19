#include "chrono.h"

#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJSEngine>
#include <QFontDatabase>
#include <QJsonDocument>
#include <QProcess>
#include <QStandardPaths>
#include <functional>

extern "C" {
#include "ini.h"
#include "theme.h"
}

static QColor qc(const chrono_rgba &c)
{
	return QColor::fromRgbF(float(c.r), float(c.g), float(c.b), float(c.a));
}

// "Tahoma, Liberation Sans, DejaVu Sans Bold 9" -> {family: first installed, size: 9, bold: true}
static QVariantMap parseFont(const QString &desc)
{
	QString d = desc.trimmed();
	double size = 10;
	bool bold = false, italic = false;
	QStringList words = d.split(QLatin1Char(' '), Qt::SkipEmptyParts);
	if (!words.isEmpty()) {
		bool ok = false;
		double s = words.last().toDouble(&ok);
		if (ok) {
			size = s;
			words.removeLast();
		}
	}
	while (!words.isEmpty()) {
		const QString w = words.last().toLower();
		if (w == QLatin1String("bold")) bold = true;
		else if (w == QLatin1String("italic")) italic = true;
		else if (w != QLatin1String("regular") && w != QLatin1String("medium")) break;
		words.removeLast();
	}
	const QStringList fams = words.join(QLatin1Char(' ')).split(QLatin1Char(','), Qt::SkipEmptyParts);
	const QStringList installed = QFontDatabase::families();
	QString family;
	for (const QString &f : fams) {
		const QString t = f.trimmed();
		for (const QString &i : installed)
			if (!i.compare(t, Qt::CaseInsensitive)) {
				family = i;
				break;
			}
		if (!family.isEmpty())
			break;
	}
	if (family.isEmpty())
		family = fams.isEmpty() ? QStringLiteral("sans-serif") : fams.last().trimmed();
	return {{QStringLiteral("family"), family}, {QStringLiteral("size"), size},
		{QStringLiteral("bold"), bold}, {QStringLiteral("italic"), italic}};
}

static const char *decorName(int d)
{
	static const char *n[] = {"flat", "bevel", "platinum", "pixel", "tab", "motif",
				  "cube", "meadow", "pinstripe", "glass", "neon"};
	return d >= 0 && d < int(sizeof n / sizeof *n) ? n[d] : "flat";
}

QVariantMap Chrono::themeToMap(const QString &pathOrId)
{
	chrono_theme t;
	const QByteArray p = pathOrId.toUtf8();
	bool ok = pathOrId.contains(QLatin1Char('/')) ? chrono_theme_load_file(&t, p.constData())
						      : chrono_theme_load(&t, p.constData());
	if (!ok)
		chrono_theme_defaults(&t);
	QVariantMap m;
#define S(k, v) m[QStringLiteral(k)] = QString::fromUtf8(v)
#define C(k, v) m[QStringLiteral(k)] = qc(t.v)
	S("id", t.id); S("name", t.name); S("era", t.era); S("description", t.description); S("path", t.path);
	C("bg", bg); C("fg", fg); C("accent", accent); C("accentFg", accent_fg); C("selection", selection);
	C("windowBg", window_bg); C("windowFg", window_fg);
	C("titleActive", title_active); C("titleActive2", title_active2); C("titleActiveFg", title_active_fg);
	C("titleInactive", title_inactive); C("titleInactive2", title_inactive2); C("titleInactiveFg", title_inactive_fg);
	C("borderActive", border_active); C("borderInactive", border_inactive);
	C("bevelLight", bevel_light); C("bevelDark", bevel_dark); C("bevelShadow", bevel_shadow);
	C("buttonBg", button_bg); C("buttonFg", button_fg); C("closeBg", close_bg);
	C("panelBg", panel_bg); C("panelFg", panel_fg); C("panelBorder", panel_border);
	C("desktop1", desktop1); C("desktop2", desktop2);
	S("panelPosition", t.panel_position); S("panelStyle", t.panel_style); S("launcherLabel", t.launcher_label);
	S("wallpaper", t.wallpaper); S("iconTheme", t.icon_theme); S("cursorTheme", t.cursor_theme);
#undef S
#undef C
	m[QStringLiteral("decor")] = QString::fromLatin1(decorName(t.decor));
	m[QStringLiteral("panelHeight")] = t.panel_height;
	m[QStringLiteral("panelDock")] = t.panel_dock;
	m[QStringLiteral("cornerRadius")] = t.corner_radius;
	m[QStringLiteral("titleHeight")] = t.title_height;
	m[QStringLiteral("borderWidth")] = t.border_width;
	m[QStringLiteral("shadow")] = t.shadow;
	m[QStringLiteral("cursorSize")] = t.cursor_size;
	m[QStringLiteral("fontUi")] = parseFont(QString::fromUtf8(t.font_ui));
	m[QStringLiteral("fontTitle")] = parseFont(QString::fromUtf8(t.font_title));
	m[QStringLiteral("fontMono")] = parseFont(QString::fromUtf8(t.font_mono));
	const QString style = QString::fromUtf8(t.panel_style);
	m[QStringLiteral("retro")] = QString::fromUtf8(t.era) == QLatin1String("retro");
	m[QStringLiteral("bevelled")] = style == QLatin1String("bevel") || style == QLatin1String("pixel");
	return m;
}

QString Chrono::socketPath()
{
	const QByteArray env = qgetenv("CHRONO_SOCK");
	if (!env.isEmpty())
		return QString::fromLocal8Bit(env);
	QString rt = QString::fromLocal8Bit(qgetenv("XDG_RUNTIME_DIR"));
	if (rt.isEmpty())
		rt = QStringLiteral("/tmp");
	QString wd = QString::fromLocal8Bit(qgetenv("WAYLAND_DISPLAY"));
	if (wd.isEmpty())
		wd = QStringLiteral("wayland-0");
	return rt + QStringLiteral("/chrono-") + wd + QStringLiteral(".sock");
}

QString Chrono::configPath()
{
	const QByteArray env = qgetenv("CHRONO_CONFIG");
	if (!env.isEmpty())
		return QString::fromLocal8Bit(env);
	return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QStringLiteral("/chrono/chrono.conf");
}

QString Chrono::outputsPath()
{
	return QFileInfo(configPath()).absolutePath() + QStringLiteral("/outputs.conf");
}

QString Chrono::version() const
{
#ifdef CHRONO_VERSION
	return QStringLiteral(CHRONO_VERSION);
#else
	return QStringLiteral("dev");
#endif
}

Chrono::Chrono(QObject *parent) : QObject(parent)
{
	connect(&m_events, &QLocalSocket::readyRead, this, &Chrono::onEventData);
	connect(&m_requests, &QLocalSocket::readyRead, this, &Chrono::onReplyData);
	connect(&m_events, &QLocalSocket::connected, this, [this] {
		m_events.write("subscribe\n");
		emit connectedChanged();
		refreshAll();
	});
	auto lost = [this] {
		emit connectedChanged();
		// drop pending callbacks: their replies will never come
		m_internal.clear();
		m_replyBuf.clear();
		m_eventBuf.clear();
		m_retry.start();
	};
	connect(&m_events, &QLocalSocket::disconnected, this, lost);
	connect(&m_events, &QLocalSocket::errorOccurred, this, [this, lost](QLocalSocket::LocalSocketError) {
		if (m_events.state() != QLocalSocket::ConnectedState)
			lost();
	});
	m_retry.setInterval(1000);
	m_retry.setSingleShot(true);
	connect(&m_retry, &QTimer::timeout, this, &Chrono::connectNow);
	m_theme = themeToMap(configValue(QStringLiteral("general.theme"), QStringLiteral("midnight")));
	m_themeId = m_theme.value(QStringLiteral("id")).toString();
	connectNow();
}

Chrono::~Chrono()
{
	// the sockets' disconnect handlers touch other members, which may already be gone
	disconnect(&m_events, nullptr, this, nullptr);
	disconnect(&m_requests, nullptr, this, nullptr);
	m_retry.stop();
}

void Chrono::connectNow()
{
	if (m_events.state() != QLocalSocket::UnconnectedState)
		m_events.abort();
	if (m_requests.state() != QLocalSocket::UnconnectedState)
		m_requests.abort();
	m_requests.connectToServer(socketPath());
	m_events.connectToServer(socketPath());
}

static QVariant parseJson(const QByteArray &line)
{
	QJsonParseError err;
	QJsonDocument d = QJsonDocument::fromJson(line, &err);
	if (err.error != QJsonParseError::NoError)
		return QVariantMap{{QStringLiteral("ok"), false}, {QStringLiteral("error"), err.errorString()}};
	return d.toVariant();
}

void Chrono::onEventData()
{
	m_eventBuf += m_events.readAll();
	int nl;
	while ((nl = m_eventBuf.indexOf('\n')) >= 0) {
		const QByteArray line = m_eventBuf.left(nl);
		m_eventBuf.remove(0, nl + 1);
		const QVariantMap ev = parseJson(line).toMap();
		if (ev.contains(QStringLiteral("event")))
			handleEvent(ev);
	}
}

void Chrono::onReplyData()
{
	m_replyBuf += m_requests.readAll();
	int nl;
	while ((nl = m_replyBuf.indexOf('\n')) >= 0) {
		const QByteArray line = m_replyBuf.left(nl);
		m_replyBuf.remove(0, nl + 1);
		if (m_internal.isEmpty())
			continue;
		auto fn = m_internal.dequeue();
		if (fn)
			fn(parseJson(line));
	}
}

void Chrono::request(const QString &line, std::function<void(const QVariant &)> fn)
{
	if (m_requests.state() != QLocalSocket::ConnectedState) {
		if (fn)
			fn(QVariantMap{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("not connected")}});
		return;
	}
	m_internal.enqueue(std::move(fn));
	m_requests.write(line.toUtf8() + '\n');
}

void Chrono::command(const QString &line, const QJSValue &callback)
{
	QJSValue cb = callback;
	request(line, [this, cb](const QVariant &r) mutable {
		if (cb.isCallable() && m_engine)
			cb.call({m_engine->toScriptValue(r)});
	});
}

QVariant Chrono::commandSync(const QString &line, int timeoutMs)
{
	QLocalSocket s;
	s.connectToServer(socketPath());
	if (!s.waitForConnected(timeoutMs))
		return QVariantMap{{QStringLiteral("ok"), false}, {QStringLiteral("error"), QStringLiteral("Chrono isn't running")}};
	s.write(line.toUtf8() + '\n');
	s.flush();
	QByteArray buf;
	while (!buf.contains('\n') && s.waitForReadyRead(timeoutMs))
		buf += s.readAll();
	return parseJson(buf.left(buf.indexOf('\n')));
}

void Chrono::refreshAll()
{
	request(QStringLiteral("windows"), [this](const QVariant &r) {
		m_windows = r.toList();
		emit windowsChanged();
	});
	request(QStringLiteral("outputs"), [this](const QVariant &r) {
		m_outputs = r.toList();
		emit outputsChanged();
	});
	request(QStringLiteral("workspaces"), [this](const QVariant &r) {
		const QVariantMap m = r.toMap();
		m_workspace = m.value(QStringLiteral("current"), 1).toInt();
		m_workspaceCount = m.value(QStringLiteral("count"), 4).toInt();
		emit workspaceChanged();
	});
	refreshTheme();
}

void Chrono::refreshTheme()
{
	request(QStringLiteral("theme"), [this](const QVariant &r) {
		const QVariantMap m = r.toMap();
		const QString path = m.value(QStringLiteral("path")).toString();
		const QString id = m.value(QStringLiteral("id")).toString();
		if (id.isEmpty())
			return;
		m_theme = themeToMap(path.isEmpty() ? id : path);
		m_themeId = id;
		emit themeChanged();
	});
}

void Chrono::refreshThemes()
{
	request(QStringLiteral("themes"), [this](const QVariant &r) {
		m_themes = r.toList();
		emit themesChanged();
	});
	if (!connected()) {
		// compositor not running: list theme files ourselves
		m_themes.clear();
		chrono_theme_list([](const char *path, const char *id, void *data) {
			auto *list = static_cast<QVariantList *>(data);
			chrono_theme t;
			if (!chrono_theme_load_file(&t, path))
				return;
			list->append(QVariantMap{{QStringLiteral("id"), QString::fromUtf8(id)},
						 {QStringLiteral("name"), QString::fromUtf8(t.name)},
						 {QStringLiteral("era"), QString::fromUtf8(t.era)},
						 {QStringLiteral("description"), QString::fromUtf8(t.description)},
						 {QStringLiteral("path"), QString::fromUtf8(path)}});
		}, &m_themes);
		emit themesChanged();
	}
}

void Chrono::handleEvent(const QVariantMap &ev)
{
	const QString e = ev.value(QStringLiteral("event")).toString();
	const QVariant data = ev.value(QStringLiteral("data"));
	if (e == QLatin1String("windows")) {
		request(QStringLiteral("windows"), [this](const QVariant &r) {
			m_windows = r.toList();
			emit windowsChanged();
		});
	} else if (e == QLatin1String("workspace")) {
		const QVariantMap m = data.toMap();
		if (m.contains(QStringLiteral("workspace")))
			m_workspace = m.value(QStringLiteral("workspace")).toInt();
		else if (m.contains(QStringLiteral("current")))
			m_workspace = m.value(QStringLiteral("current")).toInt();
		if (m.contains(QStringLiteral("count")))
			m_workspaceCount = m.value(QStringLiteral("count")).toInt();
		emit workspaceChanged();
		request(QStringLiteral("windows"), [this](const QVariant &r) {
			m_windows = r.toList();
			emit windowsChanged();
		});
	} else if (e == QLatin1String("outputs")) {
		request(QStringLiteral("outputs"), [this](const QVariant &r) {
			m_outputs = r.toList();
			emit outputsChanged();
		});
	} else if (e == QLatin1String("theme")) {
		refreshTheme();
	} else if (e == QLatin1String("config")) {
		emit configChanged();
		request(QStringLiteral("workspaces"), [this](const QVariant &r) {
			const QVariantMap m = r.toMap();
			m_workspace = m.value(QStringLiteral("current"), 1).toInt();
			m_workspaceCount = m.value(QStringLiteral("count"), 4).toInt();
			emit workspaceChanged();
		});
		refreshTheme();
	} else if (e == QLatin1String("launcher")) {
		emit launcherRequested();
	} else if (e == QLatin1String("power-menu")) {
		emit powerMenuRequested();
	} else if (e == QLatin1String("notifications")) {
		emit notificationsRequested();
	} else if (e == QLatin1String("osd")) {
		emit osdRequested(data.toMap().value(QStringLiteral("kind")).toString());
	} else if (e == QLatin1String("window-menu")) {
		emit windowMenuRequested(data.toMap());
	} else if (e == QLatin1String("locked")) {
		emit locked();
	} else if (e == QLatin1String("unlocked")) {
		emit unlocked();
	}
}

void Chrono::spawn(const QString &cmd)
{
	QProcess::startDetached(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), cmd});
}

QVariantMap Chrono::readConfig() const
{
	QVariantMap m;
	chrono_ini ini = {};
	const QByteArray p = configPath().toLocal8Bit();
	if (chrono_ini_load(&ini, p.constData()))
		for (size_t i = 0; i < ini.count; i++)
			m.insert(QString::fromUtf8(ini.entries[i].key), QString::fromUtf8(ini.entries[i].value));
	chrono_ini_free(&ini);
	return m;
}

QString Chrono::configValue(const QString &key, const QString &def) const
{
	return readConfig().value(key.toLower(), def).toString();
}

bool Chrono::setConfig(const QString &section, const QString &key, const QString &value)
{
	// Through the compositor when it runs (it writes the file and applies it), else straight to the file.
	if (connected()) {
		const QVariantMap r = commandSync(QStringLiteral("set %1 %2 %3").arg(section, key, value)).toMap();
		if (r.value(QStringLiteral("ok")).toBool())
			return true;
	}
	QDir().mkpath(QFileInfo(configPath()).absolutePath());
	const QByteArray p = configPath().toLocal8Bit(), s = section.toUtf8(), k = key.toUtf8(), v = value.toUtf8();
	const bool ok = chrono_ini_set_in_file(p.constData(), s.constData(), k.constData(), v.constData());
	emit configChanged();
	return ok;
}

QVariantMap Chrono::readOutputsConfig() const
{
	QVariantMap m;
	chrono_ini ini = {};
	const QByteArray p = outputsPath().toLocal8Bit();
	if (chrono_ini_load(&ini, p.constData()))
		for (size_t i = 0; i < ini.count; i++)
			m.insert(QString::fromUtf8(ini.entries[i].key), QString::fromUtf8(ini.entries[i].value));
	chrono_ini_free(&ini);
	return m;
}
