<div align="center">

# Meridian Store

**A KDE-native graphical software store for Arch Linux.**

Official Arch repositories · an optional curated `[meridian]` repo · Flatpak · AUR —
in one store, each source clearly trust-labelled.

</div>

---

## What is this?

Meridian Store closes Arch's one real gap against Fedora/Ubuntu: a clean,
app-store-style software manager — not “yet another pacman frontend”. It reads
the AppStream catalog (icons, screenshots, categories) and installs/updates
software from:

- **Official Arch repos** (`core`/`extra`/…) — signed, via pacman
- **`[meridian]`** — an *optional* curated, signed repo, shown as its own lane
  (just a normal pacman repo; absent on a stock system, the badge simply doesn’t appear)
- **Flatpak** (Flathub) — on by default, toggleable
- **AUR** — the unvetted lane: **off by default**, opt-in, and every build is
  pinned to the exact commit you reviewed

It originates in **MeridianOS** (an Arch + KDE Plasma system) but runs on
**any Arch Linux + KDE** install.

## Architecture

```
┌──────────────────────────────────────────────┐
│  meridian-store   Qt6/QML + Kirigami (user)    │  unprivileged
│  catalog via AppStreamQt · libalpm read-only   │
└───────────────┬────────────────────────────────┘
                │ D-Bus (session → system), polkit-gated
        ┌───────┴────────────────────────────────┐
        │  meridian-stored   Rust system daemon    │  root, polkit
        │  pacman transactions · AUR clean-chroot   │
        └────────────────────────────────────────────┘
```

The GUI is unprivileged; every mutating action crosses D-Bus and is gated by
polkit in the `meridian-stored` daemon. Package names are validated against
flag/shell injection; AUR builds run in a clean `devtools` chroot, **pinned to
the reviewed commit** (no review/build TOCTOU).

## Build

Dependencies (host):

```bash
sudo pacman -S --needed git rust cargo cmake extra-cmake-modules ninja \
  qt6-base qt6-declarative kirigami ki18n kstatusnotifieritem \
  appstream appstream-qt archlinux-appstream-data pacman pacman-contrib \
  polkit flatpak python python-numpy python-pillow librsvg
```

GUI:

```bash
cmake -S meridian-store -B meridian-store/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build meridian-store/build
```

Daemon:

```bash
cargo build --release --locked --manifest-path meridian-stored/Cargo.toml
```

### Package (Arch)

A ready `PKGBUILD` lives in [`packaging/`](packaging/PKGBUILD); it builds both
components from a tagged release. Set the source URL + checksum, then `makepkg`.

## Licence & trademarks

Code is **GPL-3.0-or-later** (see [LICENSE](LICENSE)).

Meridian Store targets **Arch Linux** and is built with **KDE Frameworks**
(Kirigami). It is an independent, **unofficial** project: “Arch Linux” is a
trademark of the Arch Linux project, which does not endorse or operate it. KDE
and Plasma are trademarks of KDE e.V.
