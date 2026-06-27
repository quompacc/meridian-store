#pragma once

#include <QObject>

class KStatusNotifierItem;
class Updates;

// Update notifier in the system tray. It is an *update notifier*, not a permanent
// resident: the tray item only exists while updates are pending (then it shows
// the update icon). With nothing to install there is no tray entry at all. The
// background watcher (`meridian-store --background`) keeps the process alive and
// re-checks on a timer, so the item appears on its own when updates land.
// Activating it / its menu raises the window — QML wires `activated`/`showUpdates`.
class TrayIcon : public QObject
{
    Q_OBJECT
public:
    // `updates` drives visibility; the tray follows it on every changed().
    explicit TrayIcon(Updates *updates, QObject *parent = nullptr);

Q_SIGNALS:
    void activated();    // raise the store window (landing)
    void showUpdates();  // raise + open the updates page

private:
    void refresh();
    void createItem();   // register the tray item (updates appeared)
    void destroyItem();  // remove it (system back to up-to-date)

    KStatusNotifierItem *m_sni = nullptr;
    Updates *m_updates;
};
