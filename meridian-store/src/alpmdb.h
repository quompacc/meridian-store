#pragma once

#include <QString>

// Read-only view of the local pacman database (libalpm): "is this installed?"
// plus "which repo does it come from?" The sync DBs (registered from
// /etc/pacman.conf) let the UI label the trust lane — official Arch repos vs.
// the curated [meridian] repo — instead of lumping them together.
class AlpmDb
{
public:
    AlpmDb();
    ~AlpmDb();

    AlpmDb(const AlpmDb &) = delete;
    AlpmDb &operator=(const AlpmDb &) = delete;

    bool isInstalled(const QString &pkgName) const;

    // The sync repo a package is provided by (e.g. "core", "extra", "meridian"),
    // or an empty string if it isn't in any registered sync DB. First match wins,
    // mirroring pacman's repo precedence (order in pacman.conf).
    QString repoOf(const QString &pkgName) const;

private:
    void registerSyncDbs(); // parse /etc/pacman.conf, register each [repo]

    void *m_handle = nullptr;   // alpm_handle_t*
    void *m_localdb = nullptr;  // alpm_db_t*
};
