#include "trayicon.h"

#include "updates.h"

#include <QAction>
#include <QApplication>
#include <QMenu>

#include <KLocalizedString>
#include <KStatusNotifierItem>

TrayIcon::TrayIcon(Updates *updates, QObject *parent)
    : QObject(parent)
    , m_updates(updates)
{
    connect(m_updates, &Updates::changed, this, &TrayIcon::refresh);
    refresh();
}

void TrayIcon::createItem()
{
    if (m_sni) {
        return;
    }
    m_sni = new KStatusNotifierItem(QStringLiteral("org.meridian.store"), this);
    m_sni->setCategory(KStatusNotifierItem::ApplicationStatus);
    m_sni->setStatus(KStatusNotifierItem::Active);
    m_sni->setTitle(i18n("Meridian Store"));
    m_sni->setIconByName(QStringLiteral("org.meridian.store-updates"));
    // We ship our own entries — don't let KSNI add a second (standard) Quit.
    m_sni->setStandardActionsEnabled(false);

    // KSNI takes ownership of the menu and deletes it with the item, so build a
    // fresh one each time the item is (re)created.
    auto *menu = new QMenu();
    auto *showUpd = menu->addAction(QIcon::fromTheme(QStringLiteral("system-software-update")),
                                    i18n("Aktualisierungen anzeigen"));
    connect(showUpd, &QAction::triggered, this, &TrayIcon::showUpdates);

    auto *open = menu->addAction(QIcon::fromTheme(QStringLiteral("applications-all")),
                                 i18n("Store öffnen"));
    connect(open, &QAction::triggered, this, &TrayIcon::activated);

    auto *recheck = menu->addAction(QIcon::fromTheme(QStringLiteral("view-refresh")),
                                    i18n("Nach Updates suchen"));
    connect(recheck, &QAction::triggered, m_updates, &Updates::check);

    menu->addSeparator();
    auto *quit = menu->addAction(QIcon::fromTheme(QStringLiteral("application-exit")),
                                 i18n("Beenden"));
    connect(quit, &QAction::triggered, qApp, &QApplication::quit);

    m_sni->setContextMenu(menu);

    // The item exists only to flag updates → primary click opens the updates page.
    connect(m_sni, &KStatusNotifierItem::activateRequested, this,
            [this](bool /*active*/, const QPoint &) { Q_EMIT showUpdates(); });
}

void TrayIcon::destroyItem()
{
    if (!m_sni) {
        return;
    }
    delete m_sni; // unregisters from the tray (and deletes the owned menu)
    m_sni = nullptr;
}

void TrayIcon::refresh()
{
    const int n = m_updates->count();
    if (n > 0) {
        createItem();
        m_sni->setToolTip(QStringLiteral("org.meridian.store-updates"), i18n("Meridian Store"),
                          i18np("%1 Aktualisierung verfügbar",
                                "%1 Aktualisierungen verfügbar", n));
    } else {
        destroyItem();
    }
}
