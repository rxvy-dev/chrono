#include "notifications.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDateTime>
#include <QFile>
#include <QImage>
#include <QStandardPaths>
#include <QDBusArgument>
#include <QDir>

Notifications::Notifications(QObject *parent) : QObject(parent)
{
	auto bus = QDBusConnection::sessionBus();
	if (!bus.isConnected())
		return;
	bus.registerObject(QStringLiteral("/org/freedesktop/Notifications"), this,
			   QDBusConnection::ExportAllSlots | QDBusConnection::ExportAllSignals);
	m_registered = bus.registerService(QStringLiteral("org.freedesktop.Notifications"));
	if (!m_registered)
		qWarning("chrono-shell: another notification daemon is running; Chrono notifications are off");
}

void Notifications::setDnd(bool d)
{
	if (d == m_dnd)
		return;
	m_dnd = d;
	emit dndChanged();
}

QStringList Notifications::GetCapabilities()
{
	return {QStringLiteral("body"), QStringLiteral("body-markup"), QStringLiteral("actions"),
		QStringLiteral("icon-static"), QStringLiteral("persistence"), QStringLiteral("body-hyperlinks")};
}

QString Notifications::GetServerInformation(QString &vendor, QString &version, QString &spec)
{
	vendor = QStringLiteral("Chrono");
	version = QStringLiteral("0.1");
	spec = QStringLiteral("1.2");
	return QStringLiteral("chrono-shell");
}

// Raw image hints (image-data / icon_data) are saved to a cache file QML can show.
static QString saveImageHint(const QVariant &v, uint id)
{
	const QDBusArgument arg = v.value<QDBusArgument>();
	int w = 0, h = 0, stride = 0, bps = 0, ch = 0;
	bool alpha = false;
	QByteArray data;
	arg.beginStructure();
	arg >> w >> h >> stride >> alpha >> bps >> ch >> data;
	arg.endStructure();
	if (w <= 0 || h <= 0 || data.size() < stride * h)
		return {};
	QImage img(reinterpret_cast<const uchar *>(data.constData()), w, h, stride,
		   alpha ? QImage::Format_RGBA8888 : QImage::Format_RGB888);
	const QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/notify");
	QDir().mkpath(dir);
	const QString p = dir + QStringLiteral("/%1.png").arg(id);
	img.copy().save(p);
	return p;
}

uint Notifications::Notify(const QString &app, uint replaces, const QString &icon, const QString &summary,
			   const QString &body, const QStringList &actions, const QVariantMap &hints, int timeout)
{
	const uint id = replaces ? replaces : m_next++;
	QString image = icon;
	for (const char *k : {"image-data", "image_data", "icon_data"})
		if (hints.contains(QLatin1String(k))) {
			const QString p = saveImageHint(hints.value(QLatin1String(k)), id);
			if (!p.isEmpty())
				image = p;
			break;
		}
	if (hints.contains(QStringLiteral("image-path")))
		image = hints.value(QStringLiteral("image-path")).toString();
	if (image.startsWith(QLatin1String("file://")))
		image = image.mid(7);
	const int urgency = hints.value(QStringLiteral("urgency"), 1).toInt();
	QVariantList acts;
	for (int i = 0; i + 1 < actions.size(); i += 2)
		acts << QVariantMap{{QStringLiteral("id"), actions[i]}, {QStringLiteral("label"), actions[i + 1]}};
	const QVariantMap n{
		{QStringLiteral("id"), id}, {QStringLiteral("app"), app}, {QStringLiteral("icon"), image},
		{QStringLiteral("summary"), summary}, {QStringLiteral("body"), body}, {QStringLiteral("actions"), acts},
		{QStringLiteral("urgency"), urgency}, {QStringLiteral("time"), QDateTime::currentDateTime()},
		{QStringLiteral("timeout"), (urgency >= 2 || timeout == 0) ? 0 : (timeout > 0 ? qBound(1500, timeout, 60000) : 6000)},
	};
	auto upsert = [&](QVariantList &list) {
		for (QVariant &v : list)
			if (v.toMap().value(QStringLiteral("id")).toUInt() == id) {
				v = n;
				return;
			}
		list.prepend(n);
	};
	if (!hints.value(QStringLiteral("transient")).toBool()) {
		upsert(m_history);
		while (m_history.size() > 100)
			m_history.removeLast();
		emit historyChanged();
	}
	if (!m_dnd || urgency >= 2) {
		upsert(m_popups);
		while (m_popups.size() > 5)
			m_popups.removeLast();
		emit popupsChanged();
		if (urgency < 2 && timeout != 0) {
			QTimer *t = m_timers.value(id);
			if (!t) {
				t = new QTimer(this);
				t->setSingleShot(true);
				connect(t, &QTimer::timeout, this, [this, id] { close(id, 1); });
				m_timers.insert(id, t);
			}
			t->start(timeout > 0 ? qBound(1500, timeout, 60000) : 6000);
		}
	}
	return id;
}

void Notifications::dismiss(uint id)
{
	for (int i = 0; i < m_popups.size(); i++)
		if (m_popups[i].toMap().value(QStringLiteral("id")).toUInt() == id) {
			m_popups.removeAt(i);
			emit popupsChanged();
			break;
		}
	if (QTimer *t = m_timers.take(id))
		t->deleteLater();
}

void Notifications::close(uint id, uint reason)
{
	dismiss(id);
	emit NotificationClosed(id, reason);
}

void Notifications::CloseNotification(uint id)
{
	close(id, 3);
	remove(id);
}

void Notifications::remove(uint id)
{
	dismiss(id);
	for (int i = 0; i < m_history.size(); i++)
		if (m_history[i].toMap().value(QStringLiteral("id")).toUInt() == id) {
			m_history.removeAt(i);
			emit historyChanged();
			break;
		}
}

void Notifications::clearHistory()
{
	m_history.clear();
	emit historyChanged();
}

void Notifications::invoke(uint id, const QString &action)
{
	emit ActionInvoked(id, action);
	close(id, 2);
}
