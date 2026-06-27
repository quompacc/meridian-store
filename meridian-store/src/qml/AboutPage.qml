// Über den Meridian Store: macht sichtbar, was das Produkt ist — ein
// Software-Store für Arch Linux — samt Version, Unterbau und Marken-Hinweis.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: page
    title: i18nc("@title", "Über")

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing

            Kirigami.Icon {
                source: "org.meridian.store"
                fallback: "system-software-install"
                Layout.preferredWidth: Kirigami.Units.iconSizes.huge
                Layout.preferredHeight: Kirigami.Units.iconSizes.huge
                Layout.alignment: Qt.AlignTop
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing
                Kirigami.Heading { text: i18n("Meridian Store"); level: 1 }
                QQC2.Label {
                    text: i18n("Software-Store für Arch Linux")
                    opacity: 0.8
                }
                QQC2.Label {
                    text: i18n("Version %1", Qt.application.version)
                    font: Kirigami.Theme.smallFont
                    opacity: 0.7
                }
            }
        }

        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            text: i18n("Der Meridian Store ist der grafische Software-Store von MeridianOS, " +
                       "einem auf Arch Linux und KDE Plasma aufbauenden System. Er installiert " +
                       "und aktualisiert Software aus den offiziellen Arch-Linux-Repositorien, " +
                       "dem kuratierten [meridian]-Repositorium, Flatpak (Flathub) und dem AUR.")
        }

        Kirigami.Separator { Layout.fillWidth: true }

        QQC2.Label {
            Layout.fillWidth: true
            text: i18n("Unterbau")
            font.weight: Font.DemiBold
        }
        QQC2.Label {
            Layout.fillWidth: true
            wrapMode: Text.WordWrap
            font: Kirigami.Theme.smallFont
            opacity: 0.8
            text: i18n("Gebaut auf Arch Linux · Oberfläche mit KDE Frameworks (Kirigami) · " +
                       "Lizenz GPL-3.0-or-later.")
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: true
            type: Kirigami.MessageType.Information
            text: i18n("MeridianOS ist ein eigenständiges, inoffizielles Projekt. „Arch Linux“ " +
                       "ist eine Marke des Arch-Linux-Projekts; MeridianOS wird von Arch Linux " +
                       "weder unterstützt noch betrieben. KDE und Plasma sind Marken der KDE e.V.")
        }
    }
}
