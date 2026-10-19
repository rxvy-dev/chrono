// Turns a QML Window into a layer-shell surface (panel, dock, popup, notification...).
// Call Layer.setup(window, {...}) from Component.onCompleted, before the window becomes visible.
#pragma once
#include <QObject>
#include <QVariantMap>

class QWindow;
class QScreen;

class Layer : public QObject
{
	Q_OBJECT
public:
	enum Anchor { Top = 1, Bottom = 2, Left = 4, Right = 8 };
	Q_ENUM(Anchor)
	using QObject::QObject;

	// opts: layer ("background"|"bottom"|"top"|"overlay"), anchors (bitmask), exclusive (int),
	//       keyboard ("none"|"exclusive"|"ondemand"), scope (string), margins ([t, r, b, l]), screen (QScreen)
	Q_INVOKABLE void setup(QWindow *w, const QVariantMap &opts);
	Q_INVOKABLE void setExclusive(QWindow *w, int zone);
	Q_INVOKABLE void setAnchors(QWindow *w, int anchors);
	Q_INVOKABLE void setMargins(QWindow *w, int top, int right, int bottom, int left);
	Q_INVOKABLE void setKeyboard(QWindow *w, const QString &mode);
	static QScreen *toScreen(const QVariant &v);
};
