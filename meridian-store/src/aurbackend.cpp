#include "aurbackend.h"

#include <algorithm>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QSettings>
#include <QUrl>
#include <QUrlQuery>

namespace
{
constexpr auto RPC = "https://aur.archlinux.org/rpc/v5/search/";
constexpr auto PKGBUILD_URL = "https://aur.archlinux.org/cgit/aur.git/plain/PKGBUILD";
// Atom feed of the package branch; its newest entry links the HEAD commit id,
// which we pin the build to.
constexpr auto ATOM_URL = "https://aur.archlinux.org/cgit/aur.git/atom/";
}

// ---- AurModel --------------------------------------------------------------

AurModel::AurModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int AurModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_results.size());
}

QVariant AurModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_results.size()) {
        return {};
    }
    const AurResult &r = m_results.at(index.row());
    switch (role) {
    case NameRole:
        return r.name;
    case PkgBaseRole:
        return r.pkgBase;
    case VersionRole:
        return r.version;
    case DescriptionRole:
        return r.description;
    case MaintainerRole:
        return r.maintainer.isEmpty() ? QStringLiteral("(verwaist)") : r.maintainer;
    case VotesRole:
        return r.votes;
    case OutOfDateRole:
        return r.outOfDate;
    default:
        return {};
    }
}

QHash<int, QByteArray> AurModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {PkgBaseRole, "pkgBase"},
        {VersionRole, "version"},
        {DescriptionRole, "description"},
        {MaintainerRole, "maintainer"},
        {VotesRole, "votes"},
        {OutOfDateRole, "outOfDate"},
    };
}

void AurModel::setResults(QList<AurResult> results)
{
    beginResetModel();
    m_results = std::move(results);
    endResetModel();
}

// ---- AurBackend ------------------------------------------------------------

AurBackend::AurBackend(QObject *parent)
    : QObject(parent)
    , m_net(new QNetworkAccessManager(this))
    , m_model(new AurModel(this))
{
    // AUR is a known malware vector; off until the user explicitly opts in.
    m_enabled = QSettings().value(QStringLiteral("aur/enabled"), false).toBool();
}

void AurBackend::setEnabled(bool enabled)
{
    if (m_enabled == enabled) {
        return;
    }
    m_enabled = enabled;
    QSettings().setValue(QStringLiteral("aur/enabled"), enabled);
    if (!enabled) {
        m_model->setResults({});
        setStatus(QString());
    }
    Q_EMIT enabledChanged();
}

void AurBackend::search(const QString &term)
{
    if (!m_enabled) {
        setStatus(tr("AUR ist deaktiviert."));
        return;
    }
    const QString trimmed = term.trimmed();
    if (trimmed.length() < 2) {
        m_model->setResults({});
        setStatus(tr("Mindestens 2 Zeichen eingeben."));
        return;
    }
    setBusy(true);
    setStatus(tr("Suche im AUR …"));

    QUrl url(QString::fromLatin1(RPC) + QString::fromUtf8(QUrl::toPercentEncoding(trimmed)));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("by"), QStringLiteral("name-desc"));
    url.setQuery(query);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("meridian-store/0.1"));
    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        setBusy(false);
        if (reply->error() != QNetworkReply::NoError) {
            setStatus(tr("Netzwerkfehler: %1").arg(reply->errorString()));
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonArray results = doc.object().value(QStringLiteral("results")).toArray();
        QList<AurResult> list;
        list.reserve(results.size());
        for (const QJsonValue &v : results) {
            const QJsonObject o = v.toObject();
            AurResult r;
            r.name = o.value(QStringLiteral("Name")).toString();
            r.pkgBase = o.value(QStringLiteral("PackageBase")).toString();
            r.version = o.value(QStringLiteral("Version")).toString();
            r.description = o.value(QStringLiteral("Description")).toString();
            r.maintainer = o.value(QStringLiteral("Maintainer")).toString();
            r.votes = o.value(QStringLiteral("NumVotes")).toInt();
            r.outOfDate = !o.value(QStringLiteral("OutOfDate")).isNull();
            list.append(r);
        }
        // Most-voted first — a rough popularity/trust proxy.
        std::sort(list.begin(), list.end(),
                  [](const AurResult &a, const AurResult &b) { return a.votes > b.votes; });
        m_model->setResults(std::move(list));
        setStatus(tr("%1 Treffer im AUR (ungeprüft)").arg(m_model->rowCount()));
    });
}

void AurBackend::fetchPkgbuild(const QString &pkgBase)
{
    // Step 1: resolve the package branch's current commit, so the bytes we show
    // and the tree the daemon builds are pinned to the SAME revision (no TOCTOU).
    QUrl url(QString::fromLatin1(ATOM_URL));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("h"), pkgBase);
    url.setQuery(query);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("meridian-store/0.1"));
    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, pkgBase]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            Q_EMIT pkgbuildFetched(pkgBase, QString(),
                                   tr("# Commit konnte nicht aufgelöst werden:\n# %1")
                                       .arg(reply->errorString()));
            return;
        }
        // The first commit id in the feed is HEAD of the package branch.
        static const QRegularExpression re(QStringLiteral("id=([0-9a-f]{40})"));
        const QRegularExpressionMatch m = re.match(QString::fromUtf8(reply->readAll()));
        if (!m.hasMatch()) {
            Q_EMIT pkgbuildFetched(pkgBase, QString(),
                                   tr("# Konnte die AUR-Revision nicht bestimmen "
                                      "(Paket vorhanden?)."));
            return;
        }
        fetchPkgbuildAt(pkgBase, m.captured(1));
    });
}

void AurBackend::fetchPkgbuildAt(const QString &pkgBase, const QString &rev)
{
    // Step 2: fetch the PKGBUILD at exactly `rev` (not branch HEAD), so the text
    // the user reviews matches the commit the build is pinned to.
    QUrl url(QString::fromLatin1(PKGBUILD_URL));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("id"), rev);
    url.setQuery(query);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("meridian-store/0.1"));
    QNetworkReply *reply = m_net->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, pkgBase, rev]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            Q_EMIT pkgbuildFetched(pkgBase, QString(),
                                   tr("# PKGBUILD konnte nicht geladen werden:\n# %1")
                                       .arg(reply->errorString()));
            return;
        }
        Q_EMIT pkgbuildFetched(pkgBase, rev, QString::fromUtf8(reply->readAll()));
    });
}

void AurBackend::setBusy(bool busy)
{
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    Q_EMIT busyChanged();
}

void AurBackend::setStatus(const QString &status)
{
    if (m_status == status) {
        return;
    }
    m_status = status;
    Q_EMIT statusChanged();
}
