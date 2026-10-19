#include "preview.h"
#include "chrono.h"

#include <QDateTime>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QPainter>
#include <QPainterPath>
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

QImage GlassProvider::requestImage(const QString &rawId, QSize *size, const QSize &)
{
	const QString full = QUrl::fromPercentEncoding(rawId.section(QLatin1Char('#'), 0, 0).toUtf8());
	const QString id = full.section(QLatin1Char('?'), 0, 0);
	QHash<QString, int> v;
	for (const QString &kv : full.section(QLatin1Char('?'), 1).split(QLatin1Char('&'), Qt::SkipEmptyParts))
		v.insert(kv.section(QLatin1Char('='), 0, 0), kv.section(QLatin1Char('='), 1).toInt());
	const int sw = qBound(64, v.value(QStringLiteral("sw"), 1920), 8192), sh = qBound(64, v.value(QStringLiteral("sh"), 1080), 8192);
	const int w = qBound(1, v.value(QStringLiteral("w"), 300), sw), h = qBound(1, v.value(QStringLiteral("h"), 300), sh);
	const int x = v.value(QStringLiteral("x")), y = v.value(QStringLiteral("y")), r = qBound(0, v.value(QStringLiteral("r")), 64);

	// blurred full-screen wallpaper, cached per theme, size and config change
	static QMutex lock;
	static QHash<QString, QImage> cache;
	const QString cfg = Chrono::configPath();
	const QString key = QStringLiteral("%1|%2x%3|%4").arg(id).arg(sw).arg(sh).arg(QFileInfo(cfg).lastModified().toMSecsSinceEpoch());
	QImage blurred;
	{
		QMutexLocker l(&lock);
		blurred = cache.value(key);
	}
	if (blurred.isNull()) {
		chrono_theme t;
		const QByteArray p = id.toUtf8(), c = cfg.toLocal8Bit();
		if (!chrono_theme_load(&t, p.constData()))
			chrono_theme_defaults(&t);
		chrono_theme_apply_overrides(&t, c.constData());
		// paint small, then scale up smoothly in two steps: a cheap, soft blur
		const int bw = qMax(16, sw / 10), bh = qMax(16, sh / 10);
		QImage small(bw, bh, QImage::Format_ARGB32_Premultiplied);
		small.fill(Qt::black);
		cairo_surface_t *s = cairo_image_surface_create_for_data(small.bits(), CAIRO_FORMAT_ARGB32, bw, bh, int(small.bytesPerLine()));
		cairo_t *cr = cairo_create(s);
		chrono_paint_desktop(cr, &t, bw, bh);
		cairo_destroy(cr);
		cairo_surface_flush(s);
		cairo_surface_destroy(s);
		QImage tiny = small.scaled(qMax(8, bw / 3), qMax(8, bh / 3), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		blurred = tiny.scaled(bw, bh, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
				  .scaled(sw, sh, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		QMutexLocker l(&lock);
		if (cache.size() > 6)
			cache.clear();
		cache.insert(key, blurred);
	}
	QImage out(w, h, QImage::Format_ARGB32_Premultiplied);
	out.fill(Qt::transparent);
	QPainter pt(&out);
	pt.setRenderHint(QPainter::Antialiasing);
	QPainterPath path;
	path.addRoundedRect(QRectF(0, 0, w, h), r, r);
	pt.setClipPath(path);
	pt.drawImage(QPoint(-x, -y), blurred);
	pt.end();
	if (size)
		*size = out.size();
	return out;
}
