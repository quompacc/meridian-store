# MeridianOS — Software-Store (Paketmanager mit GUI)

> Status: **Planung / Phase 0.** Ersetzt langfristig Plasma Discover als
> grafischen Software-Manager (`packages/meridian-desktop-meta` nennt Discover
> ausdrücklich „interim … a bespoke Meridian package GUI is a later addition").

## Ziel

Das einzige echte Manko von Arch gegenüber Fedora/Ubuntu schließen: ein
**sauberer App-Store**. Nicht „noch ein pacman-Frontend", sondern ein Store, der
- den **kuratierten, signierten `[meridian]`-Katalog** als erste Quelle zeigt,
- offizielle Arch-Repos, **Flatpak** (Flathub) und **AUR** als weitere Spuren
  vereint — mit klarer Vertrauens-Kennzeichnung pro Quelle,
- sich wie ein Store anfühlt (Icons, Screenshots, Kategorien, Beschreibungen),
  nicht wie eine Paketliste,
- in MeridianOS *durchgängig* aussieht (Design-Tokens, KDE-nativ).

## Designentscheidungen (festgeklopft)

| Entscheidung | Wahl | Begründung |
|---|---|---|
| Umfang v1 | offizielle Repos + `[meridian]` + **AUR + Flatpak** | voller Ersatz, kein Halbschritt |
| GUI-Stack | **Qt6/QML + Kirigami (C++)** | KDE-nativ, gleiche Toolchain wie der Rest, über Tokens themebar |
| Katalog/Metadaten | **AppStreamQt** | Qt-nativer Katalog (Icons/Screenshots/Kategorien); kein Eigenbau |
| Transaktions-Daemon | **Rust + `alpm`-crate**, D-Bus (zbus), polkit | sicher, modern; pacman-Logik nicht per CLI scrapen |
| Privileg-Modell | GUI **unprivilegiert** → polkit-vermittelter **`meridian-stored`** | genau das macht der Arch-PackageKit-Backend falsch |

## Architektur

```
┌────────────────────────────────────────────────┐
│  meridian-store   (Qt6/QML + Kirigami, user)     │
│  - AppStreamQt  → Katalog: Icons/Screenshots/    │
│                   Kategorien/Suche/Detailseiten   │
│  - QML-Views über Design-Tokens gethemt           │
└───────────────┬──────────────────────────────────┘
                │ D-Bus (Session→System), Signale: Fortschritt/Download
        ┌───────┴───────────────────────────────────┐
        │  meridian-stored   (Rust, System-Daemon)    │  ← polkit-Gate
        │  Backends hinter einer Schnittstelle:        │
        │   • alpm   offizielle Repos + [meridian]     │
        │   • flatpak  libflatpak (Flathub)            │
        │   • aur    RPC-Suche + PKGBUILD-Review +     │
        │            makepkg in sauberem nspawn-chroot │
        └──────────────────────────────────────────────┘
```

**Vertrauens-Spuren** (im UI sichtbar gekennzeichnet):
1. `[meridian]` — kuratiert, signiert, gevettet (siehe Vetting-Policy §6.3).
2. offizielle Arch-Repos — signiert (pacman-keyring).
3. Flatpak/Flathub — sandboxed, eigene Signatur/Remotes.
4. **AUR — ungeprüft**: PKGBUILD wird vor dem Build *angezeigt* und muss bestätigt
   werden; Build läuft isoliert. Niemals stillschweigend.

## Phasen

- **P0 — Fundament. ✅ erledigt.** Devtools (`git`, Rust, `cmake`,
  `extra-cmake-modules`, `ninja`), Projekt-Skelett unter `store/meridian-store/`,
  CMake/Ninja-Build, Kirigami-Fenster startet (Meridian-getheltet, Wayland).
- **P1 — Katalog (read-only). ✅ erledigt.** AppStreamQt-Pool (1336 Desktop-Apps
  aus core/extra/multilib) → Kategorien-Sidebar, Suche, Karten-Grid mit Icons +
  Beschreibungen, Detailseite. Installations-Status read-only via `libalpm`
  (`src/alpmdb.cpp`). Auf dem Host visuell verifiziert.
  *(Update-Erkennung braucht Sync-DB-Vergleich → kommt mit dem Daemon in P2.)*
  KF6-Hinweise: `KLocalizedQmlContext` statt `KLocalizedContext`;
  `Kirigami.CardsGridView` existiert in KF6 nicht mehr → normales `GridView` mit
  `AbstractCard`-Delegates.
- **P2 — Daemon + Transaktionen. ✅ erledigt + verifiziert.**
  (busctl: `Install cowsay`→true→installiert, `Remove`→sauber entfernt,
  Flag-Injection `-Rns bash`→abgelehnt vor polkit/pacman.)
  `meridian-stored` (Rust, `zbus` 4, `zbus_polkit`): System-Bus-Dienst
  `org.meridian.Store`, polkit-Gate (`org.meridian.store.manage`), strikte
  Paketnamen-Validierung, single-in-flight. Methoden `Install/Remove/UpdateSystem/
  Refresh/ListUpdates`, `Progress`/`Finished`-Signale. GUI ruft via Qt-DBus
  (`src/storedaemon.cpp`) asynchron auf, streamt das Log, frischt den
  Installationsstatus nach der Transaktion auf.
  **Engine-Entscheidung (Iteration 1):** die Transaktion läuft über `pacman`
  (Referenz-Implementierung) statt einer handgeschriebenen libalpm-Transaktion —
  sicherer auf der echten Paket-DB, Engine bewusst austauschbar (native libalpm
  via `alpm`-crate, sobald in einer VM validiert).
- **P3 — Flatpak. ✅ erledigt.** Flathub-AppStream in den Pool (1.336 → 4.078
  Apps), Quellen-Erkennung via `bundle(KindFlatpak)`, Flatpak-Badge im Grid,
  Multi-Quellen-Detailseite (Repo *und* Flatpak je eigene Aktion). Backend
  (`src/flatpakbackend.cpp`) treibt das `flatpak`-CLI (System-Install über
  Flatpaks eigene polkit-Integration, kein Meridian-Daemon nötig). Katalog +
  Badges visuell verifiziert; echter App-Install bewusst nicht ausgelöst
  (große Runtime), CLI-Pfad ist Standard.
- **P4 — AUR. 🔧 Code komplett; Suche/Review verifiziert, Build-Pfad opt-in.**
  AUR-RPC-Suche + PKGBUILD-Review mit Pflicht-Bestätigung (`src/aurbackend.cpp`,
  `AurPage`/`AurDetailPage`), eigene Sidebar-Spur „AUR (ungeprüft)". Build über
  Daemon-Methode `BuildAur` → `build-aur.sh`: `git clone` + `makechrootpkg -c`
  (sauberer Chroot, NICHT auf dem Host) + `pacman -U`. Braucht `devtools`;
  echter Build-Test ausstehend (Chroot-Erstaufbau lädt base-devel).
  **AUR standardmäßig AUS** (`Aur.enabled`, persistiert via QSettings) — bekannter
  Malware-Vektor; die Spur erscheint erst nach bewusstem Einschalten unter
  Einstellungen.
- **P5 — Integration + Härtung. ✅ erledigt (Repo-Publish = Homelab-Handoff).**
  `.desktop` + Kompass-Icon → App im Launcher (lokal verifiziert);
  `packages/meridian-store/PKGBUILD` (baut GUI + Daemon, installiert polkit/
  D-Bus/systemd/Icon); **Discover → `meridian-store`** in
  `meridian-desktop-meta` (pkgrel 4). Theming läuft über den Plasma-Style
  (`org.kde.desktop`), erbt Meridian automatisch. **Repo-Härtung:**
  Staging→Stable-Gate (`repo/publish.sh` → staging default, `repo/promote.sh`),
  Key-Härtung (`repo/harden-key.sh`: Passphrase + YubiKey/Offline-Runbook),
  `docs/REPO_OPERATIONS.md` aktualisiert. Offen nur noch: Build/Publish-CI-Hook.

- **P6 — Store-Erlebnis. ✅ erledigt + visuell verifiziert (2026-06-26).** Aus
  dem „hübschen pacman-Frontend" wird ein Store.
  - **Reiche Detailseite** (`AppDetailPage.qml` + `ScreenshotGallery.qml`):
    Screenshot-Galerie mit Klick-Zoom, Langbeschreibung (RichText), Version/Datum,
    Entwickler, Lizenz, Webseiten-Link — on demand über `Catalog::details(appId)`
    (`screenshotsAll`/`releasesPlain`/`developer`/`url`).
  - **App-Store-Startseite** (`DiscoverLanding.qml`/`AppRow.qml`/`AppTile.qml`):
    Featured-Carousel + eine **kuratierte Top-App-Reihe pro Kategorie**
    (`Catalog::buildShowcase()` → `categoryModel()`; Token-Listen echter Top-Apps
    je Kategorie, aufgefüllt mit weiteren icon-reichen Apps), horizontale Reihen
    mit **sichtbarem Scrollbalken** + Mausrad-Pan, „Alle anzeigen" → Kategorie.
    Die flache Liste/Grid erscheint erst bei Kategorie/Suche (`landingMode`).
  - **Detail-Header-CTA**: prominenter Installieren/Deinstallieren-Button oben
    (bevorzugt die Repo-Quelle), Pro-Quelle-Frames bleiben darunter.
  - **Flatpak-Toggle** (Einstellungen, `FlatpakBackend.enabled`, default an):
    aus → `Catalog::setFlatpakEnabled()` filtert Nur-Flatpak-Apps aus Grid +
    Showcase (verifiziert: Spiele 871→202, nur „Repo"-Badges), Detail-Flatpak-
    Quelle aus, `Updates::startFlatpak()` übersprungen. Spiegelt das AUR-Muster.
  - **Systray** (`TrayIcon`, KStatusNotifierItem): reiner Update-Melder — das
    SNI-Item wird nur erzeugt, *während* Updates anstehen (dann Update-Icon +
    Zähler-Tooltip), sonst kein Tray-Eintrag. `--background`-Autostart prüft
    headless periodisch; `quitOnLastWindowClosed` nur im Wächter-Modus aus.
    Verifiziert via busctl: 0 Updates → kein Item; N>0 → Item mit
    `org.meridian.store-updates` + „N verfügbar".
  - **Updates ohne Root** (`Updates`): `checkupdates` (pacman-contrib) synct eine
    private DB-Kopie unter `fakeroot` → frische Liste **ohne polkit/sudo**
    (PackageKit-Äquivalent); Fallback `pacman -Qu`. Flatpak via `remote-ls
    --updates`. Timer-Recheck fürs Badge. Anwenden über Daemon `UpdateSystem`
    bzw. `FlatpakBackend::updateAll()`. `UpdatesPage.qml` mit „Erneut prüfen".
  - **Navigations-Fix**: `navigate()` poppt vor dem Push auf die Startseite zurück
    → Destinationen öffnen als *eine* Spalte statt zu stapeln.
  - **Karten-Redesign** (`AppCard.qml`): größeres Icon, Quellen-Badges,
    Installiert-Status.
  - **Build**: `QT_MAJOR_VERSION 6` in CMakeLists fixiert (qt5-base co-installiert
    → ECM leakte sonst Qt5-Targets). Deps: `kstatusnotifieritem`,
    `pacman-contrib`, `fakeroot`. pkgrel 3→5.
  - Verifiziert: Landing (Featured + Kategorie-Reihen), Spiele-Grid, Krita-Detail
    (Galerie+Metadaten), Updates-Seite, Tray-Tooltip.

## Host-Status (2026-06-25, dieser Host = MeridianOS, Plasma 6.7.1 Wayland)

Vorhanden: `libalpm.so.16`, `libflatpak.so.0`, `libappstream.so.5`,
`libAppStreamQt.so.3`, `flatpak`, `appstreamcli`, `qmake6`, `kirigami`,
`qt6-declarative`. **Fehlt:** `git`, Rust-Toolchain, `cmake`,
`extra-cmake-modules`.
