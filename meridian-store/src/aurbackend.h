#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QObject>
#include <QString>

class QNetworkAccessManager;

// One AUR search result.
struct AurResult {
    QString name;
    QString pkgBase;
    QString version;
    QString description;
    QString maintainer;
    int votes = 0;
    bool outOfDate = false;
};

class AurModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        NameRole = Qt::UserRole + 1,
        PkgBaseRole,
        VersionRole,
        DescriptionRole,
        MaintainerRole,
        VotesRole,
        OutOfDateRole,
    };
    explicit AurModel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void setResults(QList<AurResult> results);

private:
    QList<AurResult> m_results;
};

// AUR source — the *unvetted* lane. Search via the AUR RPC; fetch the raw
// PKGBUILD for mandatory review. The actual build/install is delegated to the
// privileged daemon (clean-chroot build), never run silently.
class AurBackend : public QObject
{
    Q_OBJECT
    Q_PROPERTY(AurModel *model READ model CONSTANT)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    // AUR is the unvetted lane and OFF by default — opt-in, persisted.
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
public:
    explicit AurBackend(QObject *parent = nullptr);

    AurModel *model() const { return m_model; }
    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    bool enabled() const { return m_enabled; }
    void setEnabled(bool enabled);

    Q_INVOKABLE void search(const QString &term);
    Q_INVOKABLE void fetchPkgbuild(const QString &pkgBase);

Q_SIGNALS:
    void busyChanged();
    void statusChanged();
    void enabledChanged();
    // `rev` is the resolved commit the PKGBUILD was fetched at — the build is
    // pinned to it so the reviewed bytes are exactly the bytes that get built.
    void pkgbuildFetched(const QString &pkgBase, const QString &rev, const QString &text);

private:
    void setBusy(bool busy);
    void setStatus(const QString &status);
    // Step 2 of fetchPkgbuild: once the commit is known, fetch the PKGBUILD at
    // that exact revision so display and build pin reference the same commit.
    void fetchPkgbuildAt(const QString &pkgBase, const QString &rev);

    QNetworkAccessManager *m_net;
    AurModel *m_model;
    bool m_busy = false;
    bool m_enabled = false;
    QString m_status;
};
