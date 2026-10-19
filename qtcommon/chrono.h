// Shared by chrono-shell and chrono-settings: the IPC connection to the compositor and the
// current theme as QML-friendly values.
#pragma once

#include <QJSValue>
#include <QLocalSocket>
#include <QObject>
#include <QQueue>
#include <QTimer>
#include <QVariant>
#include <functional>

class QJSEngine;

class Chrono : public QObject
{
	Q_OBJECT
	Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
	Q_PROPERTY(QVariantList windows READ windows NOTIFY windowsChanged)
	Q_PROPERTY(QVariantList outputs READ outputs NOTIFY outputsChanged)
	Q_PROPERTY(int workspace READ workspace NOTIFY workspaceChanged)
	Q_PROPERTY(int workspaceCount READ workspaceCount NOTIFY workspaceChanged)
	Q_PROPERTY(QVariantMap theme READ theme NOTIFY themeChanged)
	Q_PROPERTY(QVariantList themes READ themes NOTIFY themesChanged)
	Q_PROPERTY(QString selectedTheme READ selectedTheme NOTIFY themeChanged)   // what the user picked (may differ from theme.id in light/dark mode)
	Q_PROPERTY(QString version READ version CONSTANT)
	Q_PROPERTY(QVariantMap config READ readConfig NOTIFY configChanged)
	// where the panel actually is ("top"/"bottom") and whether the dock is on, from config + theme
	Q_PROPERTY(QString panelPosition READ panelPosition NOTIFY panelChanged)
	Q_PROPERTY(bool dockEnabled READ dockEnabled NOTIFY panelChanged)
	// "Reduce motion": QML multiplies its animation durations by this (0 or 1)
	Q_PROPERTY(int motion READ motion NOTIFY configChanged)

public:
	explicit Chrono(QObject *parent = nullptr);
	~Chrono() override;
	void setEngine(QJSEngine *e) { m_engine = e; }

	static QString socketPath();
	static QVariantMap themeToMap(const QString &pathOrId, const QString &mode = QString(), const QString &accent = QString());
	static QString configPath();
	static QString outputsPath();

	bool connected() const { return m_events.state() == QLocalSocket::ConnectedState; }
	QVariantList windows() const { return m_windows; }
	QVariantList outputs() const { return m_outputs; }
	int workspace() const { return m_workspace; }
	int workspaceCount() const { return m_workspaceCount; }
	QVariantMap theme() const { return m_theme; }
	QVariantList themes() const { return m_themes; }
	QString selectedTheme() const { return m_selectedTheme; }
	QString version() const;
	QString panelPosition() const;
	bool dockEnabled() const;
	int motion() const { return configValue(QStringLiteral("appearance.reduce_motion")) == QLatin1String("true") ? 0 : 1; }

	// Send one command line; callback(reply) runs when the JSON answer arrives.
	Q_INVOKABLE void command(const QString &line, const QJSValue &callback = QJSValue());
	// Blocking variant for simple cases (returns the parsed reply, or {ok:false} on failure).
	Q_INVOKABLE QVariant commandSync(const QString &line, int timeoutMs = 2000);
	Q_INVOKABLE void action(const QString &a) { command(QStringLiteral("action ") + a); }
	Q_INVOKABLE void windowCommand(const QString &verb, int id) { command(verb + QLatin1Char(' ') + QString::number(id)); }
	Q_INVOKABLE void refreshThemes();
	Q_INVOKABLE void spawn(const QString &cmd);

	// Config file helpers (used by settings; the compositor re-reads the file after `set`).
	Q_INVOKABLE QVariantMap readConfig() const;          // "section.key" -> value
	Q_INVOKABLE QString configValue(const QString &key, const QString &def = QString()) const;
	Q_INVOKABLE bool setConfig(const QString &section, const QString &key, const QString &value);
	Q_INVOKABLE QVariantMap readOutputsConfig() const;

signals:
	void connectedChanged();
	void windowsChanged();
	void outputsChanged();
	void workspaceChanged();
	void themeChanged();
	void themesChanged();
	void configChanged();
	void panelChanged();
	void launcherRequested();
	void powerMenuRequested();
	void notificationsRequested();
	void osdRequested(const QString &kind);
	void windowMenuRequested(const QVariantMap &info);
	void locked();
	void unlocked();

private:
	void connectNow();
	void onEventData();
	void onReplyData();
	void handleEvent(const QVariantMap &ev);
	void refreshAll();
	void refreshTheme();

	QLocalSocket m_events;   // subscribed: events only
	QLocalSocket m_requests; // command / reply, in order
	QByteArray m_eventBuf, m_replyBuf;
	QJSEngine *m_engine = nullptr;
	QQueue<std::function<void(const QVariant &)>> m_internal;
	QTimer m_retry;
	QVariantList m_windows, m_outputs, m_themes;
	QVariantMap m_theme;
	QString m_themeId, m_selectedTheme;
	int m_workspace = 1, m_workspaceCount = 4;
	void request(const QString &line, std::function<void(const QVariant &)> fn);
};
