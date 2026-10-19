// chrono-settings: themes, panel & dock, displays, time & date, keyboard, mouse & touchpad,
// windows, shortcuts, sound, power, default apps, autostart, about.
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <cstdio>

#include "apps.h"
#include "backend.h"
#include "chrono.h"
#include "icons.h"
#include "preview.h"

int main(int argc, char **argv)
{
	if (qEnvironmentVariableIsEmpty("QT_FORCE_STDERR_LOGGING"))
		qputenv("QT_FORCE_STDERR_LOGGING", "1");
	QGuiApplication app(argc, argv);
	app.setApplicationName(QStringLiteral("chrono-settings"));
	app.setOrganizationName(QStringLiteral("chrono"));
	app.setDesktopFileName(QStringLiteral("chrono-settings"));
	QQuickStyle::setStyle(QStringLiteral("Fusion"));

	Chrono chrono;
	Apps apps;
	Backend backend;
	IconProvider::setThemeName(chrono.theme().value(QStringLiteral("iconTheme")).toString());
	app.setWindowIcon(QIcon::fromTheme(QStringLiteral("preferences-system")));

	QQmlApplicationEngine engine;
	chrono.setEngine(&engine);
	engine.addImageProvider(QStringLiteral("icon"), new IconProvider);
	engine.addImageProvider(QStringLiteral("preview"), new PreviewProvider(false));
	engine.addImageProvider(QStringLiteral("wallpaper"), new PreviewProvider(true));
	auto *ctx = engine.rootContext();
	ctx->setContextProperty(QStringLiteral("Chrono"), &chrono);
	ctx->setContextProperty(QStringLiteral("Apps"), &apps);
	ctx->setContextProperty(QStringLiteral("Backend"), &backend);
	// open a page directly: chrono-settings displays
	ctx->setContextProperty(QStringLiteral("startPage"), app.arguments().value(1));
	engine.load(QUrl(QStringLiteral("qrc:/qml/main.qml")));
	if (engine.rootObjects().isEmpty()) {
		fprintf(stderr, "chrono-settings: the QML failed to load (see the errors above)\n");
		return 1;
	}
	return app.exec();
}
