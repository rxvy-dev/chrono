#include "icons.h"

#include <QDir>
#include <QFile>
#include <QIcon>
#include <QStandardPaths>
#include <QUrl>

void IconProvider::setThemeName(const QString &name)
{
	if (QIcon::fallbackThemeName().isEmpty())
		QIcon::setFallbackThemeName(QStringLiteral("hicolor"));
	if (!name.isEmpty())
		QIcon::setThemeName(name);
	else if (QIcon::themeName().isEmpty() || QIcon::themeName() == QLatin1String("hicolor"))
		QIcon::setThemeName(QStringLiteral("Adwaita"));
}

static QIcon lookup(const QString &raw)
{
	const QString name = QUrl::fromPercentEncoding(raw.toUtf8());
	if (name.startsWith(QLatin1Char('/')) && QFile::exists(name))
		return QIcon(name);
	QIcon i = QIcon::fromTheme(name);
	if (!i.isNull())
		return i;
	// lots of apps ship their icon only in /usr/share/pixmaps
	for (const QString &ext : {QStringLiteral(".png"), QStringLiteral(".svg"), QStringLiteral(".xpm")}) {
		const QString p = QStringLiteral("/usr/share/pixmaps/") + name + ext;
		if (QFile::exists(p))
			return QIcon(p);
	}
	// reverse-DNS app ids often match their icon only by the last part ("org.gnome.Nautilus" -> "nautilus")
	const QString last = name.section(QLatin1Char('.'), -1).toLower();
	if (last != name) {
		i = QIcon::fromTheme(last);
		if (!i.isNull())
			return i;
	}
	i = QIcon::fromTheme(name.toLower());
	if (!i.isNull())
		return i;
	return QIcon::fromTheme(QStringLiteral("application-x-executable"),
				QIcon::fromTheme(QStringLiteral("applications-other")));
}

QPixmap IconProvider::requestPixmap(const QString &id, QSize *size, const QSize &requested)
{
	const int s = requested.width() > 0 ? requested.width() : 32;
	QPixmap p = lookup(id).pixmap(s, s);
	if (p.isNull()) {
		p = QPixmap(s, s);
		p.fill(Qt::transparent);
	}
	if (size)
		*size = p.size();
	return p;
}
