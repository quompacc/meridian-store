#pragma once

#include <QObject>
#include <QString>

// Flatpak source. Drives the `flatpak` CLI directly from the (unprivileged) GUI.
// System installs use Flatpak's *own* polkit integration — no Meridian daemon
// involved. This is the standard app-store pattern (as in Discover/GNOME Software).
class FlatpakBackend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString log READ log NOTIFY logChanged)
    Q_PROPERTY(bool available READ available CONSTANT)
    // Flatpak source toggle — ON by default, persisted. Off hides the Flatpak
    // lane (catalog entries, install actions, update checks).
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
public:
    explicit FlatpakBackend(QObject *parent = nullptr);

    bool busy() const { return m_busy; }
    QString log() const { return m_log; }
    bool available() const { return m_available; }
    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);

    Q_INVOKABLE void install(const QString &appId);
    Q_INVOKABLE void remove(const QString &appId);
    Q_INVOKABLE void updateAll();
    Q_INVOKABLE void clearLog();
    // Live check (user or system installation).
    Q_INVOKABLE bool isInstalled(const QString &appId) const;

Q_SIGNALS:
    void busyChanged();
    void logChanged();
    void enabledChanged();
    void finished(bool success, const QString &message);

private:
    void run(const QStringList &args);
    void setBusy(bool busy);
    void appendLog(const QString &line);

    bool m_busy = false;
    bool m_available = false;
    bool m_enabled = true;
    QString m_log;
};
