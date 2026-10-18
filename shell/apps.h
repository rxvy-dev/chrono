// Installed applications (XDG .desktop files), search for the launcher, pinned dock apps,
// and app_id -> icon/name lookups for the taskbar.
#pragma once
#include <QFileSystemWatcher>
#include <QObject>
#include <QTimer>
#include <QVariant>

class Apps : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QVariantList all READ all NOTIFY changed)
	Q_PROPERTY(QStringList pinned READ pinned NOTIFY pinnedChanged)
	Q_PROPERTY(QVariantList categories READ categories NOTIFY changed)
public:
	explicit Apps(QObject *parent = nullptr);
	QVariantList all() const { return m_list; }
	QStringList pinned() const { return m_pinned; }
	QVariantList categories() const;

	Q_INVOKABLE QVariantList search(const QString &text, int limit = 60) const;
	Q_INVOKABLE QVariantList inCategory(const QString &cat) const;
	Q_INVOKABLE QVariantMap app(const QString &id) const;
	Q_INVOKABLE QVariantMap forAppId(const QString &appId) const; // match a window to its .desktop
	Q_INVOKABLE void launch(const QString &id, const QString &action = QString());
	Q_INVOKABLE void runCommand(const QString &cmd);
	Q_INVOKABLE void pin(const QString &id);
	Q_INVOKABLE void unpin(const QString &id);
	Q_INVOKABLE bool isPinned(const QString &id) const { return m_pinned.contains(id); }
	Q_INVOKABLE void movePinned(int from, int to);

signals:
	void changed();
	void pinnedChanged();

private:
	void scan();
	void savePrefs();
	void loadPrefs();
	QVariantList m_list;
	QHash<QString, int> m_index;
	QHash<QString, int> m_usage;
	QStringList m_pinned;
	QFileSystemWatcher m_watch;
	QTimer m_rescan;
};
