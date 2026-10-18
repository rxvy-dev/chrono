// chrono-shell: panel, dock, launcher, notifications, OSD, power and window menus for Chrono.
// Every window is a layer-shell surface; all desktop state comes from the compositor over IPC.
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>

#include "apps.h"
#include "chrono.h"
#include "icons.h"
#include "layer.h"
#include "notifications.h"
#include "preview.h"
#include "system.h"

int main(int argc, char **argv)
{
	// all of our windows are layer surfaces (needed by layer-shell-qt 5.x; harmless on 6.x)
	qputenv("QT_WAYLAND_SHELL_INTEGRATION", "layer-shell");
	if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
		qputenv("QT_QPA_PLATFORM", "wayland");
	QQuickWindow::setDefaultAlphaBuffer(true);
	QGuiApplication app(argc, argv);
	app.setApplicationName(QStringLiteral("chrono-shell"));
	app.setOrganizationName(QStringLiteral("chrono"));
	app.setQuitOnLastWindowClosed(false);

	Chrono chrono;
	Apps apps;
	System system;
	Notifications notifications;
	Layer layer;

	IconProvider::setThemeName(chrono.theme().value(QStringLiteral("iconTheme")).toString());
	QObject::connect(&chrono, &Chrono::themeChanged, &app, [&chrono] {
		IconProvider::setThemeName(chrono.theme().value(QStringLiteral("iconTheme")).toString());
	});
	QObject::connect(&chrono, &Chrono::osdRequested, &system, [&system](const QString &kind) {
		if (kind == QLatin1String("brightness"))
			QTimer::singleShot(120, &system, &System::refreshBrightness);
		else
			QTimer::singleShot(120, &system, &System::refreshAudio);
	});

	QQmlApplicationEngine engine;
	chrono.setEngine(&engine);
	engine.addImageProvider(QStringLiteral("icon"), new IconProvider);
	engine.addImageProvider(QStringLiteral("wallpaper"), new PreviewProvider(true));
	auto *ctx = engine.rootContext();
	ctx->setContextProperty(QStringLiteral("Chrono"), &chrono);
	ctx->setContextProperty(QStringLiteral("Apps"), &apps);
	ctx->setContextProperty(QStringLiteral("System"), &system);
	ctx->setContextProperty(QStringLiteral("Notifications"), &notifications);
	ctx->setContextProperty(QStringLiteral("Layer"), &layer);
	engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
	if (engine.rootObjects().isEmpty())
		return 1;
	return app.exec();
}
