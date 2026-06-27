#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QObject>
#include <QString>

class QProcess;
class QTimer;

// One pending update, from either the pacman sync DBs or Flatpak.
struct UpdateEntry {
    QString name;
    QString oldVersion;
    QString newVersion;
    QString source; // "repo" | "flatpak"
};

class UpdatesModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        OldVersionRole,
        NewVersionRole,
        SourceRole,
    };

    explicit UpdatesModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEntries(QList<UpdateEntry> entries);

private:
    QList<UpdateEntry> m_entries;
};

// Unprivileged update check — no sudo, no polkit. Native updates via
// `checkupdates` (pacman-contrib): it syncs a *user-owned copy* of the pacman DB
// under fakeroot and diffs against it, so it never touches /var/lib/pacman/sync.
// Falls back to plain `pacman -Qu` (stale but root-free) when checkupdates is
// absent. Flatpak updates via `flatpak remote-ls --updates`. Applying updates is
// delegated: repo → the polkit daemon (UpdateSystem), flatpak → the backend.
// Re-checks itself on a timer so the tray badge stays current in the background.
class Updates : public QObject
{
    Q_OBJECT
    Q_PROPERTY(UpdatesModel *model READ model CONSTANT)
    Q_PROPERTY(int count READ count NOTIFY changed)
    Q_PROPERTY(int repoCount READ repoCount NOTIFY changed)
    Q_PROPERTY(int flatpakCount READ flatpakCount NOTIFY changed)
    Q_PROPERTY(bool checking READ checking NOTIFY checkingChanged)
public:
    explicit Updates(QObject *parent = nullptr);

    UpdatesModel *model() const { return m_model; }
    int count() const { return m_repo.size() + m_flatpak.size(); }
    int repoCount() const { return m_repo.size(); }
    int flatpakCount() const { return m_flatpak.size(); }
    bool checking() const { return m_checking; }

    // (Re-)scan both sources. Asynchronous; emits changed() when done.
    Q_INVOKABLE void check();

Q_SIGNALS:
    void changed();
    void checkingChanged();

private:
    void startNative();
    void startFlatpak();
    void publish();
    void setChecking(bool checking);

    UpdatesModel *m_model;
    QTimer *m_poll;
    QList<UpdateEntry> m_repo;
    QList<UpdateEntry> m_flatpak;
    bool m_checking = false;
};
