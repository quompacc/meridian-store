// Einstellungen — v.a. das bewusste Aktivieren der ungeprüften AUR-Spur.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: page
    title: i18nc("@title", "Einstellungen")

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        Kirigami.Heading { text: i18n("Quellen"); level: 4 }

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                QQC2.Label { text: i18n("Flatpak aktivieren"); font.weight: Font.DemiBold }
                QQC2.Label {
                    text: i18n("Flathub als Quelle. Standardmäßig an.")
                    font: Kirigami.Theme.smallFont
                    opacity: 0.7
                }
            }
            QQC2.Switch {
                checked: Flatpak.enabled
                onToggled: Flatpak.enabled = checked
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                QQC2.Label { text: i18n("AUR aktivieren"); font.weight: Font.DemiBold }
                QQC2.Label {
                    text: i18n("Ungeprüfter Community-Code. Standardmäßig aus.")
                    font: Kirigami.Theme.smallFont
                    opacity: 0.7
                }
            }
            QQC2.Switch {
                checked: Aur.enabled
                onToggled: Aur.enabled = checked
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: true
            type: Kirigami.MessageType.Information
            text: Flatpak.enabled
                ? i18n("Flatpak ist aktiv. Flathub-Apps erscheinen im Katalog und werden bei Updates mitgesucht.")
                : i18n("Flatpak ist deaktiviert. Nur-Flatpak-Apps sind ausgeblendet und Flatpak-Updates werden nicht gesucht.")
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: true
            type: Aur.enabled ? Kirigami.MessageType.Warning : Kirigami.MessageType.Information
            text: Aur.enabled
                ? i18n("AUR ist aktiv. Pakete werden nicht von Arch geprüft — jeden PKGBUILD vor dem Bauen lesen.")
                : i18n("AUR ist deaktiviert. Die Spur erscheint erst in der Seitenleiste, wenn du sie hier einschaltest.")
        }
    }
}
