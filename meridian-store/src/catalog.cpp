#include "catalog.h"

#include <QDateTime>
#include <QMap>
#include <QProcess>
#include <QSet>
#include <QSettings>
#include <QUrl>
#include <QVector>
#include <algorithm>

#include <AppStreamQt/bundle.h>
#include <AppStreamQt/component-box.h>
#include <AppStreamQt/developer.h>
#include <AppStreamQt/icon.h>
#include <AppStreamQt/image.h>
#include <AppStreamQt/release.h>
#include <AppStreamQt/release-list.h>
#include <AppStreamQt/screenshot.h>

namespace
{
// Only let through web URLs we are willing to fetch/open. AppStream metadata is
// distro-provided but still external input: a poisoned entry must not make us
// load cleartext http images or hand a non-web scheme to the browser/opener.
bool isSafeWebUrl(const QUrl &url)
{
    return url.isValid()
        && (url.scheme() == QLatin1String("https") || url.scheme() == QLatin1String("http"));
}

// One cheap call: every installed flatpak app id (user + system installations).
QSet<QString> queryInstalledFlatpaks()
{
    QSet<QString> set;
    QProcess p;
    p.start(QStringLiteral("/usr/bin/flatpak"),
            {QStringLiteral("list"), QStringLiteral("--app"),
             QStringLiteral("--columns=application")});
    if (p.waitForFinished(4000)) {
        const QString out = QString::fromUtf8(p.readAllStandardOutput());
        const auto lines = out.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            set.insert(line.trimmed());
        }
    }
    return set;
}
}

// ---- CatalogModel ----------------------------------------------------------

CatalogModel::CatalogModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int CatalogModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_entries.size());
}

QVariant CatalogModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }
    const AppEntry &e = m_entries.at(index.row());
    switch (role) {
    case IdRole:
        return e.id;
    case NameRole:
        return e.name;
    case SummaryRole:
        return e.summary;
    case IconRole:
        return e.icon;
    case PkgNameRole:
        return e.pkgName;
    case RepoRole:
        return e.repo;
    case FlatpakRefRole:
        return e.flatpakRef;
    case InstalledRole:
        return e.installed;
    case CategoriesRole:
        return e.categories;
    default:
        return {};
    }
}

QHash<int, QByteArray> CatalogModel::roleNames() const
{
    return {
        {IdRole, "appId"},
        {NameRole, "name"},
        {SummaryRole, "summary"},
        {IconRole, "icon"},
        {PkgNameRole, "pkgName"},
        {RepoRole, "repo"},
        {FlatpakRefRole, "flatpakRef"},
        {InstalledRole, "installed"},
        {CategoriesRole, "categories"},
    };
}

void CatalogModel::setEntries(QList<AppEntry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}

// ---- Catalog ---------------------------------------------------------------

Catalog::Catalog(QObject *parent)
    : QObject(parent)
    , m_model(new CatalogModel(this))
    , m_featured(new CatalogModel(this))
{
    m_flatpakEnabled = QSettings().value(QStringLiteral("flatpak/enabled"), true).toBool();
    m_flatpakInstalled = queryInstalledFlatpaks();
    m_loaded = m_pool.load();
    Q_EMIT loadedChanged();
    buildShowcase();
    rebuild();
}

CatalogModel *Catalog::categoryModel(const QString &category) const
{
    return m_categoryModels.value(category, nullptr);
}

void Catalog::setFlatpakEnabled(bool enabled)
{
    if (m_flatpakEnabled == enabled) {
        return;
    }
    m_flatpakEnabled = enabled;
    buildShowcase(); // rebuilds the landing models in place
    rebuild();       // re-filters the grid/search model
}

void Catalog::setCategory(const QString &category)
{
    if (m_category == category) {
        return;
    }
    m_category = category;
    rebuild();
}

void Catalog::setSearch(const QString &search)
{
    const QString trimmed = search.trimmed();
    if (m_search == trimmed) {
        return;
    }
    m_search = trimmed;
    rebuild();
}

bool Catalog::isPkgInstalled(const QString &pkgName) const
{
    AlpmDb db; // fresh handle → reflects the on-disk localdb right now
    return db.isInstalled(pkgName);
}

void Catalog::rebuild()
{
    AppStream::ComponentBox box = !m_search.isEmpty()
        ? m_pool.search(m_search)
        : (m_category.isEmpty() ? m_pool.components()
                                : m_pool.componentsByCategories({m_category}));

    QList<AppEntry> entries;
    QSet<QString> seen;
    for (const AppStream::Component &c : box) {
        if (c.kind() != AppStream::Component::KindDesktopApp) {
            continue;
        }
        // When searching within a category, keep only matches in that category.
        if (!m_search.isEmpty() && !m_category.isEmpty() && !c.categories().contains(m_category)) {
            continue;
        }
        const QString id = c.id();
        if (seen.contains(id)) {
            continue;
        }
        AppEntry e = toEntry(c);
        // Flatpak lane off → hide apps that are *only* installable via Flatpak.
        if (!m_flatpakEnabled && e.pkgName.isEmpty() && !e.flatpakRef.isEmpty()) {
            continue;
        }
        seen.insert(id);
        entries.append(std::move(e));
    }

    // Search results come pre-ranked; otherwise sort alphabetically.
    if (m_search.isEmpty()) {
        std::sort(entries.begin(), entries.end(), [](const AppEntry &a, const AppEntry &b) {
            return a.name.localeAwareCompare(b.name) < 0;
        });
    }

    m_model->setEntries(std::move(entries));
    Q_EMIT filterChanged();
}

AppEntry Catalog::toEntry(const AppStream::Component &component) const
{
    AppEntry e;
    e.id = component.id();
    e.name = component.name();
    e.summary = component.summary();
    e.categories = component.categories();
    e.pkgName = component.packageNames().value(0);
    e.repo = e.pkgName.isEmpty() ? QString() : m_alpm.repoOf(e.pkgName);

    const AppStream::Bundle flatpak = component.bundle(AppStream::Bundle::KindFlatpak);
    if (!flatpak.isEmpty()) {
        // For flatpak components the AppStream id is the flatpak app id.
        e.flatpakRef = component.id();
    }

    const bool nativeInstalled = !e.pkgName.isEmpty() && m_alpm.isInstalled(e.pkgName);
    const bool flatpakInstalled =
        !e.flatpakRef.isEmpty() && m_flatpakInstalled.contains(e.flatpakRef);
    e.installed = nativeInstalled || flatpakInstalled;

    // Prefer the largest cached/local/remote pixmap; fall back to a stock name.
    QString bestUrl;
    uint bestWidth = 0;
    QString stockName;
    const QList<AppStream::Icon> icons = component.icons();
    for (const AppStream::Icon &ic : icons) {
        if (ic.kind() == AppStream::Icon::KindStock) {
            if (stockName.isEmpty()) {
                stockName = ic.name();
            }
        } else {
            const QUrl url = ic.url();
            if (isSafeWebUrl(url) && ic.width() >= bestWidth) {
                bestWidth = ic.width();
                bestUrl = url.toString();
            }
        }
    }
    if (!bestUrl.isEmpty()) {
        e.icon = bestUrl;
    } else if (!stockName.isEmpty()) {
        e.icon = stockName;
    } else {
        e.icon = QStringLiteral("package-x-generic");
    }
    return e;
}

namespace
{
// Largest source screenshot URL for a component, or empty if none.
QStringList screenshotUrls(const AppStream::Component &c, int max)
{
    QStringList urls;
    const QList<AppStream::Screenshot> shots = c.screenshotsAll();
    for (const AppStream::Screenshot &s : shots) {
        QUrl best;
        uint bestW = 0;
        for (const AppStream::Image &img : s.imagesAll()) {
            // Prefer the source image (full size) over thumbnails.
            const bool isSource = img.kind() == AppStream::Image::KindSource;
            if (best.isEmpty() || isSource || img.width() > bestW) {
                if (isSafeWebUrl(img.url())) {
                    best = img.url();
                    bestW = img.width();
                    if (isSource) {
                        break;
                    }
                }
            }
        }
        if (best.isValid()) {
            urls.append(best.toString());
        }
        if (urls.size() >= max) {
            break;
        }
    }
    return urls;
}
}

QVariantMap Catalog::details(const QString &appId) const
{
    QVariantMap d;
    if (appId.isEmpty()) {
        return d;
    }
    AppStream::ComponentBox box = m_pool.componentsById(appId);
    if (box.isEmpty()) {
        return d;
    }
    const AppStream::Component c = box.indexSafe(0).value_or(AppStream::Component());

    const QString pkgName = c.packageNames().value(0);
    d.insert(QStringLiteral("repo"), pkgName.isEmpty() ? QString() : m_alpm.repoOf(pkgName));
    d.insert(QStringLiteral("description"), c.description());
    d.insert(QStringLiteral("license"), c.projectLicense());
    d.insert(QStringLiteral("developer"), c.developer().name());
    d.insert(QStringLiteral("categories"), c.categories());

    const QUrl homepage = c.url(AppStream::Component::UrlKindHomepage);
    d.insert(QStringLiteral("homepage"), isSafeWebUrl(homepage) ? homepage.toString() : QString());

    const AppStream::ReleaseList releases = c.releasesPlain();
    const auto rel = releases.indexSafe(0);
    if (rel) {
        d.insert(QStringLiteral("version"), rel->version());
        const QDateTime ts = rel->timestamp();
        d.insert(QStringLiteral("releaseDate"),
                 ts.isValid() ? ts.date().toString(Qt::ISODate) : QString());
    } else {
        d.insert(QStringLiteral("version"), QString());
        d.insert(QStringLiteral("releaseDate"), QString());
    }

    d.insert(QStringLiteral("screenshots"), screenshotUrls(c, 6));
    return d;
}

void Catalog::buildShowcase()
{
    // Curated featured row. Match these tokens (case-insensitive substring)
    // against a component's id / package name / display name. Filled up with any
    // visually-rich app (remote icon + a screenshot) so the row is never empty.
    static const QStringList kCurated = {
        QStringLiteral("firefox"),     QStringLiteral("chromium"),
        QStringLiteral("vlc"),         QStringLiteral("gimp"),
        QStringLiteral("blender"),     QStringLiteral("krita"),
        QStringLiteral("inkscape"),    QStringLiteral("libreoffice"),
        QStringLiteral("thunderbird"), QStringLiteral("obs"),
        QStringLiteral("kdenlive"),    QStringLiteral("audacity"),
        QStringLiteral("signal"),      QStringLiteral("steam"),
        QStringLiteral("okular"),      QStringLiteral("mpv"),
        QStringLiteral("darktable"),   QStringLiteral("handbrake"),
    };
    // Curated *top apps* per main category, in display order. Matched the same
    // way as featured (lowercase substring against id + pkg + name). Whatever the
    // pool actually carries lands first, in this order; the row is then topped up
    // with other icon-rich apps so it never looks empty. Keep tokens distinctive
    // to avoid false hits (e.g. "code - oss", not bare "code").
    static const QHash<QString, QStringList> kCuratedByCat = {
        {QStringLiteral("AudioVideo"), {
            QStringLiteral("vlc"), QStringLiteral("mpv"), QStringLiteral("obs"),
            QStringLiteral("audacity"), QStringLiteral("kdenlive"), QStringLiteral("shotcut"),
            QStringLiteral("handbrake"), QStringLiteral("clementine"), QStringLiteral("strawberry"),
            QStringLiteral("ardour"), QStringLiteral("haruna"), QStringLiteral("elisa"),
            QStringLiteral("rhythmbox"), QStringLiteral("spotify")}},
        {QStringLiteral("Development"), {
            QStringLiteral("code - oss"), QStringLiteral("codium"), QStringLiteral("qt creator"),
            QStringLiteral("kdevelop"), QStringLiteral("geany"), QStringLiteral("neovim"),
            QStringLiteral("emacs"), QStringLiteral("kate"), QStringLiteral("meld"),
            QStringLiteral("android studio"), QStringLiteral("arduino"), QStringLiteral("dbeaver"),
            QStringLiteral("gitg"), QStringLiteral("zed")}},
        {QStringLiteral("Game"), {
            QStringLiteral("steam"), QStringLiteral("lutris"), QStringLiteral("0 a.d"),
            QStringLiteral("supertuxkart"), QStringLiteral("minetest"), QStringLiteral("wesnoth"),
            QStringLiteral("retroarch"), QStringLiteral("dolphin"), QStringLiteral("pcsx2"),
            QStringLiteral("openttd"), QStringLiteral("prismlauncher"), QStringLiteral("mindustry"),
            QStringLiteral("heroic"), QStringLiteral("supertux")}},
        {QStringLiteral("Graphics"), {
            QStringLiteral("gimp"), QStringLiteral("inkscape"), QStringLiteral("krita"),
            QStringLiteral("blender"), QStringLiteral("darktable"), QStringLiteral("digikam"),
            QStringLiteral("rawtherapee"), QStringLiteral("scribus"), QStringLiteral("mypaint"),
            QStringLiteral("gwenview"), QStringLiteral("shotwell"), QStringLiteral("freecad")}},
        {QStringLiteral("Network"), {
            QStringLiteral("firefox"), QStringLiteral("chromium"), QStringLiteral("thunderbird"),
            QStringLiteral("signal"), QStringLiteral("telegram"), QStringLiteral("discord"),
            QStringLiteral("brave"), QStringLiteral("vivaldi"), QStringLiteral("qbittorrent"),
            QStringLiteral("transmission"), QStringLiteral("filezilla"), QStringLiteral("element"),
            QStringLiteral("librewolf"), QStringLiteral("nheko")}},
        {QStringLiteral("Office"), {
            QStringLiteral("libreoffice"), QStringLiteral("onlyoffice"), QStringLiteral("calligra"),
            QStringLiteral("okular"), QStringLiteral("evince"), QStringLiteral("gnucash"),
            QStringLiteral("kmymoney"), QStringLiteral("joplin"), QStringLiteral("obsidian"),
            QStringLiteral("zotero"), QStringLiteral("calibre"), QStringLiteral("gnumeric")}},
        {QStringLiteral("Science"), {
            QStringLiteral("octave"), QStringLiteral("scilab"), QStringLiteral("kstars"),
            QStringLiteral("stellarium"), QStringLiteral("gnuplot"), QStringLiteral("qgis"),
            QStringLiteral("geogebra"), QStringLiteral("marble"), QStringLiteral("labplot"),
            QStringLiteral("kalzium"), QStringLiteral("sagemath")}},
        {QStringLiteral("System"), {
            QStringLiteral("htop"), QStringLiteral("btop"), QStringLiteral("gparted"),
            QStringLiteral("partitionmanager"), QStringLiteral("virt-manager"), QStringLiteral("virtualbox"),
            QStringLiteral("gnome disks"), QStringLiteral("timeshift"), QStringLiteral("filelight"),
            QStringLiteral("baobab"), QStringLiteral("bleachbit"), QStringLiteral("system monitor")}},
        {QStringLiteral("Utility"), {
            QStringLiteral("keepassxc"), QStringLiteral("kcalc"), QStringLiteral("flameshot"),
            QStringLiteral("spectacle"), QStringLiteral("ark"), QStringLiteral("kfind"),
            QStringLiteral("copyq"), QStringLiteral("gnome calculator"), QStringLiteral("galculator"),
            QStringLiteral("albert"), QStringLiteral("ulauncher"), QStringLiteral("kruler")}},
    };
    const QStringList kMainCats = kCuratedByCat.keys();
    constexpr int kFeaturedMax = 12;
    constexpr int kRowMax = 16;

    QList<AppEntry> curated;       // featured, ordered by kCurated
    QList<AppEntry> rich;          // featured fallback pool
    QSet<QString> featuredSeen;
    QVector<int> curatedRank(kCurated.size(), -1);

    // Per category: curated picks keyed by their rank (so display order follows
    // the curated list), a name-sorted fallback pool, and a dedup set.
    QHash<QString, QMap<int, AppEntry>> curatedByCat;
    QHash<QString, QList<AppEntry>> fallbackByCat;
    QHash<QString, QSet<QString>> seenByCat;

    for (const AppStream::Component &c : m_pool.components()) {
        if (c.kind() != AppStream::Component::KindDesktopApp) {
            continue;
        }
        const QString id = c.id();
        const AppEntry e = toEntry(c);
        // Flatpak lane off → drop Flatpak-only apps from featured + category rows.
        if (!m_flatpakEnabled && e.pkgName.isEmpty() && !e.flatpakRef.isEmpty()) {
            continue;
        }
        const bool hasRichIcon =
            e.icon.startsWith(QLatin1String("http")) || e.icon.startsWith(QLatin1Char('/'));
        const bool hasShot = !c.screenshotsAll().isEmpty();
        const QString hay =
            (id + QLatin1Char(' ') + e.pkgName + QLatin1Char(' ') + e.name).toLower();

        // --- featured ---
        if (!featuredSeen.contains(id)) {
            int rank = -1;
            for (int i = 0; i < kCurated.size(); ++i) {
                if (hay.contains(kCurated.at(i))) {
                    rank = i;
                    break;
                }
            }
            if (rank >= 0 && curatedRank[rank] < 0) {
                featuredSeen.insert(id);
                curatedRank[rank] = curated.size();
                curated.append(e);
            } else if (hasRichIcon && hasShot && rich.size() < kFeaturedMax) {
                featuredSeen.insert(id);
                rich.append(e);
            }
        }

        // --- per-category rows: curated top apps first, then icon-rich filler ---
        for (const QString &cat : c.categories()) {
            const auto tokensIt = kCuratedByCat.find(cat);
            if (tokensIt == kCuratedByCat.end()) {
                continue; // not a main category
            }
            QSet<QString> &seen = seenByCat[cat];
            if (seen.contains(id)) {
                continue;
            }
            int rank = -1;
            const QStringList &tokens = tokensIt.value();
            for (int i = 0; i < tokens.size(); ++i) {
                if (hay.contains(tokens.at(i))) {
                    rank = i;
                    break;
                }
            }
            bool placed = false;
            if (rank >= 0 && !curatedByCat[cat].contains(rank)) {
                curatedByCat[cat].insert(rank, e);
                placed = true;
            } else if (hasRichIcon && fallbackByCat[cat].size() < kRowMax) {
                fallbackByCat[cat].append(e);
                placed = true;
            }
            if (placed) {
                seen.insert(id);
            }
        }
    }

    QList<AppEntry> featured = curated;
    for (const AppEntry &e : rich) {
        if (featured.size() >= kFeaturedMax) {
            break;
        }
        featured.append(e);
    }
    m_featured->setEntries(std::move(featured));

    for (const QString &cat : kMainCats) {
        // QMap iterates ascending by key → curated picks in their listed order.
        QList<AppEntry> list = curatedByCat.value(cat).values();
        QList<AppEntry> fallback = fallbackByCat.value(cat);
        std::sort(fallback.begin(), fallback.end(), [](const AppEntry &a, const AppEntry &b) {
            return a.name.localeAwareCompare(b.name) < 0;
        });
        for (const AppEntry &e : fallback) {
            if (list.size() >= kRowMax) {
                break;
            }
            list.append(e);
        }
        // Reuse the existing model object so QML rows bound to it refresh on toggle.
        CatalogModel *model = m_categoryModels.value(cat, nullptr);
        if (!model) {
            model = new CatalogModel(this);
            m_categoryModels.insert(cat, model);
        }
        model->setEntries(std::move(list));
    }
}
