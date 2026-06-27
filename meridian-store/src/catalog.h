#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QList>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <AppStreamQt/pool.h>

#include "alpmdb.h"

// One catalog entry, projected from an AppStream component.
struct AppEntry {
    QString id;
    QString name;
    QString summary;
    QString icon;        // themed icon name OR a file/remote URL (Kirigami.Icon eats both)
    QStringList categories;
    QString pkgName;     // native package (pacman), empty if none
    QString repo;        // sync repo providing pkgName (core/extra/meridian/…), may be empty
    QString flatpakRef;  // flatpak app id (e.g. org.videolan.VLC), empty if none
    bool installed = false;
};

// List model over the filtered catalog.
class CatalogModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        SummaryRole,
        IconRole,
        PkgNameRole,
        RepoRole,
        FlatpakRefRole,
        InstalledRole,
        CategoriesRole,
    };

    explicit CatalogModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setEntries(QList<AppEntry> entries);

private:
    QList<AppEntry> m_entries;
};

// Catalog backend: loads the AppStream pool once, then filters by category +
// search text into the model. Read-only in Phase 1 (installed state via alpm).
class Catalog : public QObject
{
    Q_OBJECT
    Q_PROPERTY(CatalogModel *model READ model CONSTANT)
    Q_PROPERTY(CatalogModel *featured READ featured CONSTANT)
    Q_PROPERTY(int featuredCount READ featuredCount CONSTANT)
    Q_PROPERTY(QString category READ category WRITE setCategory NOTIFY filterChanged)
    Q_PROPERTY(QString search READ search WRITE setSearch NOTIFY filterChanged)
    Q_PROPERTY(int count READ count NOTIFY filterChanged)
    Q_PROPERTY(bool loaded READ loaded NOTIFY loadedChanged)
public:
    explicit Catalog(QObject *parent = nullptr);

    CatalogModel *model() const { return m_model; }
    CatalogModel *featured() const { return m_featured; }
    int featuredCount() const { return m_featured->rowCount(); }
    QString category() const { return m_category; }
    QString search() const { return m_search; }
    int count() const { return m_model->rowCount(); }
    bool loaded() const { return m_loaded; }

    void setCategory(const QString &category);
    void setSearch(const QString &search);

    // Live installed-state for a single package (fresh alpm handle), so the UI
    // can refresh a card/page after a transaction without reloading the pool.
    Q_INVOKABLE bool isPkgInstalled(const QString &pkgName) const;

    // Full metadata for one component (by AppStream id), fetched on demand for
    // the detail page: long description, license, developer, homepage, latest
    // version + date, and a list of screenshot source URLs. Keys:
    // description, license, developer, homepage, version, releaseDate,
    // screenshots (QStringList), categories (QStringList).
    Q_INVOKABLE QVariantMap details(const QString &appId) const;

    // A small, icon-rich showcase model for one main category (built once at
    // load), for the store-style category rows on the landing page. Returns
    // nullptr for unknown categories.
    Q_INVOKABLE CatalogModel *categoryModel(const QString &category) const;

    // Toggle whether Flatpak-only apps appear in the catalog/showcase. Driven by
    // the Flatpak source switch; rebuilds the visible models in place.
    Q_INVOKABLE void setFlatpakEnabled(bool enabled);

Q_SIGNALS:
    void filterChanged();
    void loadedChanged();

private:
    void rebuild();
    void buildShowcase();
    AppEntry toEntry(const AppStream::Component &component) const;

    AppStream::Pool m_pool;
    CatalogModel *m_model;
    CatalogModel *m_featured;
    QHash<QString, CatalogModel *> m_categoryModels;
    AlpmDb m_alpm;
    bool m_flatpakEnabled = true;
    QSet<QString> m_flatpakInstalled; // installed flatpak app ids (user+system)
    QString m_category; // empty = all
    QString m_search;
    bool m_loaded = false;
};
