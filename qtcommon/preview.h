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

// image://glass/<theme>?sw=W&sh=H&x=X&y=Y&w=W&h=H&r=R — the wallpaper, blurred, cut to one popup's
// rectangle (with rounded corners) so glass popups look frosted. The blurred wallpaper is cached.
class GlassProvider : public QQuickImageProvider
{
public:
	GlassProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
	QImage requestImage(const QString &id, QSize *size, const QSize &requested) override;
};
