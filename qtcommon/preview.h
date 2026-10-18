// image://preview/<theme id or path>  — a mock desktop painted by the same code the compositor uses,
// so the theme gallery shows exactly what you'll get.  image://wallpaper/<id> paints only the desktop.
#pragma once
#include <QQuickImageProvider>

class PreviewProvider : public QQuickImageProvider
{
public:
	explicit PreviewProvider(bool wallpaperOnly = false)
		: QQuickImageProvider(QQuickImageProvider::Image), m_wallpaper(wallpaperOnly) {}
	QImage requestImage(const QString &id, QSize *size, const QSize &requested) override;

private:
	bool m_wallpaper;
};
