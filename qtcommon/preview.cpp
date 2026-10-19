#include "preview.h"
#include "chrono.h"

#include <QUrl>
#include <cairo.h>

extern "C" {
#include "paint.h"
#include "theme.h"
}

QImage PreviewProvider::requestImage(const QString &rawId, QSize *size, const QSize &requested)
{
	// ids may carry a cache-busting suffix: "midnight#3"
	// "id", or "id?mode=dark&accent=#ff8800" for live previews of the appearance settings
	const QString full = QUrl::fromPercentEncoding(rawId.section(QLatin1Char('#'), 0, 0).toUtf8());   // "#n" = cache-buster
	const QString id = full.section(QLatin1Char('?'), 0, 0);
	QString mode, accent;
	bool custom = false;   // apply the user's own colours from chrono.conf
	for (const QString &kv : full.section(QLatin1Char('?'), 1).split(QLatin1Char('&'), Qt::SkipEmptyParts)) {
		if (kv.startsWith(QLatin1String("mode="))) mode = kv.mid(5);
		else if (kv.startsWith(QLatin1String("accent="))) accent = kv.mid(7);
		else if (kv == QLatin1String("custom=1")) custom = true;
	}
	const int w = requested.width() > 0 ? requested.width() : 480;
	const int h = requested.height() > 0 ? requested.height() : w * 10 / 16;
	chrono_theme t;
	const QByteArray p = id.toUtf8();
	const QByteArray m = mode.toUtf8(), a = accent.toUtf8();
	bool ok = id.contains(QLatin1Char('/')) ? chrono_theme_load_file(&t, p.constData())
						: chrono_theme_load_prefs(&t, p.constData(), m.constData(), a.constData(), nullptr);
	if (!ok)
		chrono_theme_defaults(&t);
	if (custom) {
		const QByteArray cfg = Chrono::configPath().toLocal8Bit();
		chrono_theme_apply_overrides(&t, cfg.constData());
		chrono_theme_apply_accent(&t, a.constData());   // the accent goes on top of custom colours
	}
	QImage img(w, h, QImage::Format_ARGB32_Premultiplied);
	img.fill(Qt::transparent);
	cairo_surface_t *s = cairo_image_surface_create_for_data(img.bits(), CAIRO_FORMAT_ARGB32, w, h, int(img.bytesPerLine()));
	cairo_t *cr = cairo_create(s);
	if (m_wallpaper)
		chrono_paint_desktop(cr, &t, w, h);
	else
		chrono_paint_preview(cr, &t, w, h);
	cairo_destroy(cr);
	cairo_surface_flush(s);
	cairo_surface_destroy(s);
	if (size)
		*size = img.size();
	return img;
}
