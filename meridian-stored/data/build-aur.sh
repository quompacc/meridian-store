#!/usr/bin/env bash
# Wird vom Meridian-Store-Daemon (als root) für einen *bestätigten* AUR-Build
# ausgeführt. Klont den AUR-Repo, baut in einem sauberen Chroot (devtools,
# NICHT auf dem Host) und installiert das Ergebnis. Streamt nach stdout/stderr;
# der Daemon leitet das als Progress-Signale an die GUI weiter.
set -euo pipefail

pkgbase="${1:-}"
rev="${2:-}"
# Zweite Verteidigungslinie (der Daemon validiert bereits).
[[ "$pkgbase" =~ ^[a-zA-Z0-9@._+-]+$ ]] || { echo "Ungültiger pkgbase"; exit 2; }
# Die im GUI geprüfte Revision (Commit-Hash). Pflicht — ohne Pin würde ein
# anderer (evtl. bösartiger) Stand gebaut als der Nutzer gelesen hat.
[[ "$rev" =~ ^[0-9a-fA-F]{7,40}$ ]] || { echo "Ungültige Revision"; exit 2; }

command -v makechrootpkg >/dev/null || { echo "devtools fehlt (makechrootpkg)"; exit 3; }

chroot_dir=/var/lib/meridian-store/chroot
work="$(mktemp -d /tmp/meridian-aur.XXXXXX)"
trap 'rm -rf "$work"' EXIT

echo ">> Klone AUR-Repo: $pkgbase"
# Voller Klon (nicht --depth 1): nur so lässt sich exakt die geprüfte Revision
# auschecken, auch wenn der Maintainer seither weitere Commits gepusht hat.
git clone "https://aur.archlinux.org/${pkgbase}.git" "$work/$pkgbase"
cd "$work/$pkgbase"
echo ">> Checke geprüfte Revision aus: $rev"
git -c advice.detachedHead=false checkout -q "$rev" \
    || { echo "Geprüfte Revision $rev nicht gefunden — Repo verändert? Abbruch."; exit 6; }
[[ -f PKGBUILD ]] || { echo "kein PKGBUILD im Repo"; exit 4; }

if [[ ! -d "$chroot_dir/root" ]]; then
    echo ">> Erstelle einmalig einen sauberen Build-Chroot unter $chroot_dir"
    install -d "$chroot_dir"
    mkarchroot "$chroot_dir/root" base-devel
fi

echo ">> Baue in sauberem Chroot (makechrootpkg -c)"
# -c: frische Chroot-Kopie pro Build; Quellen/Abhängigkeiten innerhalb isoliert.
makechrootpkg -c -r "$chroot_dir"

echo ">> Installiere gebautes Paket"
shopt -s nullglob
built=( ./*.pkg.tar.* )
[[ ${#built[@]} -gt 0 ]] || { echo "kein Paket gebaut"; exit 5; }
pacman -U --noconfirm "${built[@]}"

echo ">> Fertig: $pkgbase"
