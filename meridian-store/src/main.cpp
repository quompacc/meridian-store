#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <KLocalizedQmlContext>
#include <KLocalizedString>

#include "aurbackend.h"
#include "catalog.h"
#include "flatpakbackend.h"
#include "storedaemon.h"
#include "trayicon.h"
#include "updates.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Background update watcher (autostart on login): no window until updates
    // land and the user opens it from the tray.
    const bool startHidden = app.arguments().contains(QStringLiteral("--background"));
    // The watcher must outlive any window it opens (it keeps polling); a normal
    // launch is an ordinary app that quits when its window is closed.
    QApplication::setQuitOnLastWindowClosed(!startHidden);

    KLocalizedString::setApplicationDomain("meridian-store");
    QApplication::setApplicationName(QStringLiteral("meridian-store"));
    QApplication::setApplicationDisplayName(QStringLiteral("Meridian Store"));
    QApplication::setApplicationVersion(QStringLiteral(MERIDIAN_STORE_VERSION));
    QApplication::setOrganizationName(QStringLiteral("MeridianOS"));
    QApplication::setDesktopFileName(QStringLiteral("org.meridian.store"));

    // KDE-native look; respects the Meridian Plasma theme.
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextObject(new KLocalizedQmlContext(&engine));

    Catalog catalog;
    engine.rootContext()->setContextProperty(QStringLiteral("Catalog"), &catalog);

    StoreDaemon daemon;
    engine.rootContext()->setContextProperty(QStringLiteral("Daemon"), &daemon);

    FlatpakBackend flatpak;
    engine.rootContext()->setContextProperty(QStringLiteral("Flatpak"), &flatpak);
    // Keep the catalog's Flatpak filtering in lockstep with the source toggle.
    QObject::connect(&flatpak, &FlatpakBackend::enabledChanged, &catalog,
                     [&catalog, &flatpak]() { catalog.setFlatpakEnabled(flatpak.enabled()); });

    AurBackend aur;
    engine.rootContext()->setContextProperty(QStringLiteral("Aur"), &aur);

    Updates updates;
    engine.rootContext()->setContextProperty(QStringLiteral("Updates"), &updates);

    TrayIcon tray(&updates);
    engine.rootContext()->setContextProperty(QStringLiteral("Tray"), &tray);
    engine.rootContext()->setContextProperty(QStringLiteral("startHidden"), startHidden);

    engine.loadFromModule("org.meridian.store", "Main");

    if (engine.rootObjects().isEmpty()) {
        return -1;
    }

    return app.exec();
}
