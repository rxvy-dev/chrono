#include "layer.h"

#include <LayerShellQt/Window>
#include <QMargins>
#include <QWindow>

using LSW = LayerShellQt::Window;

static LSW::KeyboardInteractivity kbd(const QString &m)
{
	if (m == QLatin1String("exclusive"))
		return LSW::KeyboardInteractivityExclusive;
	if (m == QLatin1String("ondemand"))
		return LSW::KeyboardInteractivityOnDemand;
	return LSW::KeyboardInteractivityNone;
}

void Layer::setup(QWindow *w, const QVariantMap &o)
{
	if (!w)
		return;
	LSW *l = LSW::get(w);
	if (!l)
		return;
	const QString layer = o.value(QStringLiteral("layer"), QStringLiteral("top")).toString();
	l->setLayer(layer == QLatin1String("background") ? LSW::LayerBackground
		    : layer == QLatin1String("bottom")   ? LSW::LayerBottom
		    : layer == QLatin1String("overlay")  ? LSW::LayerOverlay
							 : LSW::LayerTop);
	l->setAnchors(LSW::Anchors(QFlag(o.value(QStringLiteral("anchors"), 0).toInt())));
	l->setExclusiveZone(o.value(QStringLiteral("exclusive"), 0).toInt());
	l->setKeyboardInteractivity(kbd(o.value(QStringLiteral("keyboard")).toString()));
	l->setScope(o.value(QStringLiteral("scope"), QStringLiteral("chrono")).toString());
	const QVariantList m = o.value(QStringLiteral("margins")).toList();
	if (m.size() == 4)
		l->setMargins(QMargins(m[3].toInt(), m[0].toInt(), m[1].toInt(), m[2].toInt()));
}

void Layer::setExclusive(QWindow *w, int zone)
{
	if (w)
		LSW::get(w)->setExclusiveZone(zone);
}

void Layer::setAnchors(QWindow *w, int anchors)
{
	if (w)
		LSW::get(w)->setAnchors(LSW::Anchors(QFlag(anchors)));
}

void Layer::setMargins(QWindow *w, int top, int right, int bottom, int left)
{
	if (w)
		LSW::get(w)->setMargins(QMargins(left, top, right, bottom));
}

void Layer::setKeyboard(QWindow *w, const QString &mode)
{
	if (w)
		LSW::get(w)->setKeyboardInteractivity(kbd(mode));
}
