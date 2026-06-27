#include "alpmdb.h"

#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

#include <alpm.h>

AlpmDb::AlpmDb()
{
    alpm_errno_t err = ALPM_ERR_OK;
    alpm_handle_t *handle = alpm_initialize("/", "/var/lib/pacman", &err);
    if (!handle) {
        return;
    }
    m_handle = handle;
    m_localdb = alpm_get_localdb(handle);
    registerSyncDbs();
}

AlpmDb::~AlpmDb()
{
    if (m_handle) {
        alpm_release(static_cast<alpm_handle_t *>(m_handle));
    }
}

void AlpmDb::registerSyncDbs()
{
    // Parse /etc/pacman.conf for repo sections and register each as a sync DB, in
    // file order (so repoOf() honours pacman's repo precedence). We only need the
    // section names; servers/sig levels are irrelevant for read-only name lookups.
    QFile conf(QStringLiteral("/etc/pacman.conf"));
    if (!conf.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    auto *handle = static_cast<alpm_handle_t *>(m_handle);
    static const QRegularExpression section(QStringLiteral("^\\s*\\[([^\\]]+)\\]"));
    QTextStream in(&conf);
    while (!in.atEnd()) {
        const QRegularExpressionMatch m = section.match(in.readLine());
        if (!m.hasMatch()) {
            continue;
        }
        const QString name = m.captured(1).trimmed();
        if (name == QLatin1String("options")) {
            continue;
        }
        alpm_register_syncdb(handle, name.toUtf8().constData(), ALPM_SIG_USE_DEFAULT);
    }
}

bool AlpmDb::isInstalled(const QString &pkgName) const
{
    if (!m_localdb || pkgName.isEmpty()) {
        return false;
    }
    alpm_pkg_t *pkg =
        alpm_db_get_pkg(static_cast<alpm_db_t *>(m_localdb), pkgName.toUtf8().constData());
    return pkg != nullptr;
}

QString AlpmDb::repoOf(const QString &pkgName) const
{
    if (!m_handle || pkgName.isEmpty()) {
        return {};
    }
    const QByteArray name = pkgName.toUtf8();
    for (alpm_list_t *i = alpm_get_syncdbs(static_cast<alpm_handle_t *>(m_handle)); i;
         i = alpm_list_next(i)) {
        auto *db = static_cast<alpm_db_t *>(i->data);
        if (alpm_db_get_pkg(db, name.constData())) {
            return QString::fromUtf8(alpm_db_get_name(db));
        }
    }
    return {};
}
