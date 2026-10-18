#include "preview.h"

#include <QUrl>
#include <cairo.h>

extern "C" {
#include "paint.h"
#include "theme.h"
}

QImage PreviewProvider::requestImage(const QString &rawId, QSize *size, const QSize &requested)
{
	// ids may carry a cache-busting suffix: "midnight#3"
	const QString id = QUrl::fromPercentEncoding(rawId.section(QLatin1Char('#'), 0, 0).toUtf8());
	const int w = requested.width() > 0 ? requested.width() : 480;
	const int h = requested.height() > 0 ? requested.height() : w * 10 / 16;
	chrono_theme t;
	const QByteArray p = id.toUtf8();
	bool ok = id.contains(QLatin1Char('/')) ? chrono_theme_load_file(&t, p.constData()) : chrono_theme_load(&t, p.constData());
	if (!ok)
		chrono_theme_defaults(&t);
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
