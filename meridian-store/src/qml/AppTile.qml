// Compact app-store tile: big icon, name, source/install badge. Used in the
// horizontal rows on the landing page. Property names are app*-prefixed to avoid
// clashing with AbstractCard's FINAL `icon` group.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.AbstractCard {
    id: tile

    property string appName
    property string appIcon
    property string appPkg
    property string appRepo
    property string appFlatpakRef
    property bool appInstalled

    showClickFeedback: true

    contentItem: ColumnLayout {
        spacing: Kirigami.Units.smallSpacing

        Kirigami.Icon {
            source: tile.appIcon
            fallback: "package-x-generic"
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Kirigami.Units.iconSizes.huge
            Layout.preferredHeight: Kirigami.Units.iconSizes.huge
        }
        QQC2.Label {
            text: tile.appName
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            horizontalAlignment: Text.AlignHCenter
            maximumLineCount: 1
            Layout.fillWidth: true
        }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Kirigami.Units.smallSpacing / 2
            Kirigami.Icon {
                visible: tile.appInstalled
                source: "emblem-installed"
                implicitWidth: Kirigami.Units.iconSizes.small
                implicitHeight: Kirigami.Units.iconSizes.small
            }
            QQC2.Label {
                text: tile.appInstalled ? i18n("Installiert")
                    : (tile.appPkg.length === 0 ? i18n("Flatpak")
                    : (tile.appRepo === "meridian" ? i18n("[meridian]") : i18n("Arch")))
                font: Kirigami.Theme.smallFont
                opacity: 0.6
            }
        }
    }
}
