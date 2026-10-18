// org.freedesktop.Notifications server: popups plus a history list for the notification centre.
#pragma once
#include <QDBusAbstractAdaptor>
#include <QDBusVariant>
#include <QObject>
#include <QTimer>
#include <QVariant>

class Notifications : public QObject
{
	Q_OBJECT
	Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")
	Q_PROPERTY(QVariantList popups READ popups NOTIFY popupsChanged)
	Q_PROPERTY(QVariantList history READ history NOTIFY historyChanged)
	Q_PROPERTY(bool doNotDisturb READ dnd WRITE setDnd NOTIFY dndChanged)
	Q_PROPERTY(bool registered READ registered CONSTANT)
public:
	explicit Notifications(QObject *parent = nullptr);
	QVariantList popups() const { return m_popups; }
	QVariantList history() const { return m_history; }
	bool dnd() const { return m_dnd; }
	void setDnd(bool d);
	bool registered() const { return m_registered; }

	Q_INVOKABLE void dismiss(uint id);           // closes the popup, keeps it in history
	Q_INVOKABLE void remove(uint id);            // removes from history too
	Q_INVOKABLE void clearHistory();
	Q_INVOKABLE void invoke(uint id, const QString &action);

public slots: // D-Bus API
	QStringList GetCapabilities();
	uint Notify(const QString &app, uint replaces, const QString &icon, const QString &summary,
		    const QString &body, const QStringList &actions, const QVariantMap &hints, int timeout);
	void CloseNotification(uint id);
	QString GetServerInformation(QString &vendor, QString &version, QString &spec);

signals:
	void popupsChanged();
	void historyChanged();
	void dndChanged();
	// D-Bus signals
	void NotificationClosed(uint id, uint reason);
	void ActionInvoked(uint id, const QString &action);

private:
	void close(uint id, uint reason);
	QVariantList m_popups, m_history;
	uint m_next = 1;
	bool m_dnd = false, m_registered = false;
	QHash<uint, QTimer *> m_timers;
};
