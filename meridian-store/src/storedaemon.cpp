#include "storedaemon.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>

namespace
{
constexpr auto SERVICE = "org.meridian.Store";
constexpr auto PATH = "/org/meridian/Store";
constexpr auto IFACE = "org.meridian.Store1";
// pacman transactions can run for minutes — well beyond the 25 s D-Bus default.
constexpr int kCallTimeoutMs = 60 * 60 * 1000;
}

StoreDaemon::StoreDaemon(QObject *parent)
    : QObject(parent)
{
    QDBusConnection bus = QDBusConnection::systemBus();

    // The service is D-Bus activated; "registered or activatable" both count.
    if (auto *iface = bus.interface()) {
        m_available = iface->isServiceRegistered(SERVICE)
            || iface->activatableServiceNames().value().contains(QString::fromLatin1(SERVICE));
    }

    bus.connect(SERVICE, PATH, IFACE, QStringLiteral("Progress"), this,
                SLOT(onProgress(QString)));
    bus.connect(SERVICE, PATH, IFACE, QStringLiteral("Finished"), this,
                SLOT(onFinished(bool, QString)));
}

void StoreDaemon::install(const QStringList &packages)
{
    callTransaction(QStringLiteral("Install"), {packages});
}

void StoreDaemon::remove(const QStringList &packages)
{
    callTransaction(QStringLiteral("Remove"), {packages});
}

void StoreDaemon::updateSystem()
{
    callTransaction(QStringLiteral("UpdateSystem"), {});
}

void StoreDaemon::refresh()
{
    callTransaction(QStringLiteral("Refresh"), {});
}

void StoreDaemon::buildAur(const QString &pkgBase, const QString &rev)
{
    callTransaction(QStringLiteral("BuildAur"), {pkgBase, rev});
}

void StoreDaemon::clearLog()
{
    m_log.clear();
    Q_EMIT logChanged();
}

void StoreDaemon::callTransaction(const QString &method, const QVariantList &args)
{
    if (m_busy) {
        return;
    }
    setBusy(true);
    appendLog(QStringLiteral("→ %1").arg(method));

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QString::fromLatin1(SERVICE), QString::fromLatin1(PATH),
        QString::fromLatin1(IFACE), method);
    msg.setArguments(args);

    QDBusPendingCall pending =
        QDBusConnection::systemBus().asyncCall(msg, kCallTimeoutMs);
    auto *watcher = new QDBusPendingCallWatcher(pending, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this,
            [this](QDBusPendingCallWatcher *w) {
                QDBusPendingReply<bool> reply = *w;
                if (reply.isError()) {
                    // polkit denial, "already running", or activation failure.
                    onFinished(false, reply.error().message());
                }
                // Success/failure of the transaction itself arrives via Finished.
                setBusy(false);
                w->deleteLater();
            });
}

void StoreDaemon::onProgress(const QString &line)
{
    appendLog(line);
}

void StoreDaemon::onFinished(bool success, const QString &message)
{
    appendLog(success ? QStringLiteral("✓ %1").arg(message)
                       : QStringLiteral("✗ %1").arg(message));
    setBusy(false);
    Q_EMIT finished(success, message);
}

void StoreDaemon::setBusy(bool busy)
{
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    Q_EMIT busyChanged();
}

void StoreDaemon::appendLog(const QString &line)
{
    if (!m_log.isEmpty()) {
        m_log.append(QLatin1Char('\n'));
    }
    m_log.append(line);
    Q_EMIT logChanged();
}
