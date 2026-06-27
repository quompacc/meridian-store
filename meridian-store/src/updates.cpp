#include "updates.h"

#include <QFileInfo>
#include <QProcess>
#include <QSettings>
#include <QTimer>

namespace
{
constexpr auto CHECKUPDATES = "/usr/bin/checkupdates";
constexpr auto PACMAN = "/usr/bin/pacman";
constexpr auto FLATPAK = "/usr/bin/flatpak";
// Re-scan in the background every few hours so the tray badge stays honest.
constexpr int kPollIntervalMs = 3 * 60 * 60 * 1000;
}

// ---- UpdatesModel ----------------------------------------------------------

UpdatesModel::UpdatesModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int UpdatesModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_entries.size());
}

QVariant UpdatesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }
    const UpdateEntry &e = m_entries.at(index.row());
    switch (role) {
    case NameRole:
        return e.name;
    case OldVersionRole:
        return e.oldVersion;
    case NewVersionRole:
        return e.newVersion;
    case SourceRole:
        return e.source;
    default:
        return {};
    }
}

QHash<int, QByteArray> UpdatesModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {OldVersionRole, "oldVersion"},
        {NewVersionRole, "newVersion"},
        {SourceRole, "source"},
    };
}

void UpdatesModel::setEntries(QList<UpdateEntry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

// ---- Updates ---------------------------------------------------------------

Updates::Updates(QObject *parent)
    : QObject(parent)
    , m_model(new UpdatesModel(this))
    , m_poll(new QTimer(this))
{
    m_poll->setInterval(kPollIntervalMs);
    connect(m_poll, &QTimer::timeout, this, &Updates::check);
    m_poll->start();
    check(); // first scan at startup (also primes the tray badge)
}

void Updates::check()
{
    if (m_checking) {
        return;
    }
    m_repo.clear();
    m_flatpak.clear();
    setChecking(true);
    startNative();
}

void Updates::startNative()
{
    auto *p = new QProcess(this);
    p->setProcessChannelMode(QProcess::MergedChannels);
    connect(p, &QProcess::finished, this,
            [this, p](int, QProcess::ExitStatus) {
                const QString out = QString::fromUtf8(p->readAllStandardOutput());
                p->deleteLater();
                const auto lines = out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
                for (const QString &line : lines) {
                    // "name oldver -> newver"
                    const auto parts = line.split(QLatin1Char(' '), Qt::SkipEmptyParts);
                    if (parts.size() >= 4 && parts.at(2) == QLatin1String("->")) {
                        m_repo.append({parts.at(0), parts.at(1), parts.at(3), QStringLiteral("repo")});
                    }
                }
                // Native list ready — show it, then chase the (slower) flatpak list.
                publish();
                startFlatpak();
            });

    // Prefer checkupdates: it syncs a private DB copy under fakeroot, so the
    // result is *fresh* without ever needing root. Without it, fall back to a
    // plain (possibly stale) -Qu against the system sync DB — still no root.
    if (QFileInfo::exists(QString::fromLatin1(CHECKUPDATES))) {
        p->start(QString::fromLatin1(CHECKUPDATES), {});
    } else {
        p->start(QString::fromLatin1(PACMAN), {QStringLiteral("-Qu")});
    }
    if (!p->waitForStarted(2000)) {
        p->deleteLater();
        startFlatpak();
    }
}

void Updates::startFlatpak()
{
    // Honour the Flatpak source toggle: skip the (network) scan when it's off.
    if (!QSettings().value(QStringLiteral("flatpak/enabled"), true).toBool()) {
        publish();
        setChecking(false);
        return;
    }

    auto *p = new QProcess(this);
    connect(p, &QProcess::finished, this,
            [this, p](int code, QProcess::ExitStatus status) {
                if (status == QProcess::NormalExit && code == 0) {
                    const QString out = QString::fromUtf8(p->readAllStandardOutput());
                    const auto lines = out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
                    for (const QString &line : lines) {
                        const QString app = line.trimmed();
                        if (!app.isEmpty()) {
                            m_flatpak.append({app, QString(), QString(), QStringLiteral("flatpak")});
                        }
                    }
                }
                p->deleteLater();
                publish();
                setChecking(false);
            });
    p->start(QString::fromLatin1(FLATPAK),
             {QStringLiteral("remote-ls"), QStringLiteral("--updates"),
              QStringLiteral("--app"), QStringLiteral("--columns=application")});
    if (!p->waitForStarted(2000)) {
        p->deleteLater();
        publish();
        setChecking(false);
    }
}

void Updates::publish()
{
    QList<UpdateEntry> all = m_repo;
    all.append(m_flatpak);
    m_model->setEntries(all);
    Q_EMIT changed();
}

void Updates::setChecking(bool checking)
{
    if (m_checking == checking) {
        return;
    }
    m_checking = checking;
    Q_EMIT checkingChanged();
}
