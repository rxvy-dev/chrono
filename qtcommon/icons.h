// image://icon/<name or path>  — freedesktop icon lookup for QML, with sensible fallbacks.
#pragma once
#include <QQuickImageProvider>

class IconProvider : public QQuickImageProvider
{
public:
	IconProvider() : QQuickImageProvider(QQuickImageProvider::Pixmap) {}
	QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requested) override;
	static void setThemeName(const QString &name);
};
