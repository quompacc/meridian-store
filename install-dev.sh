#!/usr/bin/env bash
# Dev-Installer für den Meridian-Store-Daemon (NICHT die finale Paketierung — das
# ist P5). Legt Binary + polkit/D-Bus/systemd-Integration an System-Pfade und lädt
# die Dienste neu. Muss als root laufen.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BIN="$HERE/meridian-stored/target/release/meridian-stored"
DATA="$HERE/meridian-stored/data"

[[ $EUID -eq 0 ]] || { echo "Bitte als root ausführen (sudo)."; exit 1; }
[[ -x "$BIN" ]] || { echo "Binary fehlt: $BIN  (cargo build --release?)"; exit 1; }

install -Dm755 "$BIN"                        /usr/lib/meridian-stored
install -Dm755 "$DATA/build-aur.sh"          /usr/lib/meridian-store/build-aur.sh
install -Dm644 "$DATA/org.meridian.Store.conf"    /usr/share/dbus-1/system.d/org.meridian.Store.conf
install -Dm644 "$DATA/org.meridian.Store.service" /usr/share/dbus-1/system-services/org.meridian.Store.service
install -Dm644 "$DATA/org.meridian.store.policy"  /usr/share/polkit-1/actions/org.meridian.store.policy
install -Dm644 "$DATA/meridian-stored.service"    /usr/lib/systemd/system/meridian-stored.service

systemctl daemon-reload
# D-Bus die neue System-Bus-Policy einlesen lassen (SIGHUP, kein Restart — der
# würde die laufende Session abschießen). Aktivierungs-.service-Dateien werden
# ohnehin erst beim Aktivieren gelesen.
systemctl reload dbus 2>/dev/null || true

echo "OK — Daemon installiert. Test (als normaler User):"
echo "  busctl --system call org.meridian.Store /org/meridian/Store org.meridian.Store1 ListUpdates"
