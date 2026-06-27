#include "flatpakbackend.h"

#include <QFileInfo>
#include <QProcess>
#include <QSettings>

namespace
{
constexpr auto FLATPAK = "/usr/bin/flatpak";
constexpr auto REMOTE = "flathub";
}

FlatpakBackend::FlatpakBackend(QObject *parent)
    : QObject(parent)
{
    m_available = QFileInfo::exists(QString::fromLatin1(FLATPAK));
    // On by default (unlike AUR); the user can switch the Flatpak lane off.
    m_enabled = QSettings().value(QStringLiteral("flatpak/enabled"), true).toBool();
}

void FlatpakBackend::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    QSettings().setValue(QStringLiteral("flatpak/enabled"), enabled);
    Q_EMIT enabledChanged();
}

void FlatpakBackend::install(const QString &appId)
{
    if (!m_enabled) {
        return;
    }
    // `--` ends option parsing: an app id starting with `-` can't become a flag.
    run({QStringLiteral("install"), QStringLiteral("-y"),
         QStringLiteral("--noninteractive"), QString::fromLatin1(REMOTE),
         QStringLiteral("--"), appId});
}

void FlatpakBackend::remove(const QString &appId)
{
    run({QStringLiteral("uninstall"), QStringLiteral("-y"),
         QStringLiteral("--noninteractive"), QStringLiteral("--"), appId});
}

void FlatpakBackend::updateAll()
{
    if (!m_enabled) {
        return;
    }
    run({QStringLiteral("update"), QStringLiteral("-y"),
         QStringLiteral("--noninteractive")});
}

void FlatpakBackend::clearLog()
{
    m_log.clear();
    Q_EMIT logChanged();
}

bool FlatpakBackend::isInstalled(const QString &appId) const
{
    if (!m_available || appId.isEmpty()) {
        return false;
    }
    QProcess p;
    p.start(QString::fromLatin1(FLATPAK), {QStringLiteral("info"), appId});
    p.waitForFinished(4000);
    return p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
}

void FlatpakBackend::run(const QStringList &args)
{
    if (m_busy) {
        return;
    }
    setBusy(true);
    appendLog(QStringLiteral("→ flatpak %1").arg(args.join(QLatin1Char(' '))));

    auto *proc = new QProcess(this);
    proc->setProcessChannelMode(QProcess::MergedChannels);

    connect(proc, &QProcess::readyReadStandardOutput, this, [this, proc]() {
        const QString chunk = QString::fromUtf8(proc->readAllStandardOutput());
        const auto lines = chunk.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            appendLog(line.trimmed());
        }
    });
    connect(proc, &QProcess::finished, this,
            [this, proc](int code, QProcess::ExitStatus) {
                const bool ok = (code == 0);
                appendLog(ok ? QStringLiteral("✓ Fertig")
                             : QStringLiteral("✗ flatpak rc=%1").arg(code));
                setBusy(false);
                Q_EMIT finished(ok, ok ? QStringLiteral("Fertig")
                                       : QStringLiteral("flatpak rc=%1").arg(code));
                proc->deleteLater();
            });
    connect(proc, &QProcess::errorOccurred, this,
            [this, proc](QProcess::ProcessError) {
                appendLog(QStringLiteral("✗ flatpak konnte nicht gestartet werden"));
                setBusy(false);
                Q_EMIT finished(false, QStringLiteral("flatpak nicht startbar"));
                proc->deleteLater();
            });

    proc->start(QString::fromLatin1(FLATPAK), args);
}

void FlatpakBackend::setBusy(bool busy)
{
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    Q_EMIT busyChanged();
}

void FlatpakBackend::appendLog(const QString &line)
{
    if (line.isEmpty()) {
        return;
    }
    if (!m_log.isEmpty()) {
        m_log.append(QLatin1Char('\n'));
    }
    m_log.append(line);
    Q_EMIT logChanged();
}
