// AUR-Such-Seite (Phase 4): die *ungeprüfte* Spur. Suche über die AUR-RPC,
// Auswahl führt zur PKGBUILD-Review-Seite.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: aurPage
    title: i18nc("@title", "AUR")

    titleDelegate: Kirigami.SearchField {
        Layout.fillWidth: true
        Layout.maximumWidth: Kirigami.Units.gridUnit * 24
        placeholderText: i18n("AUR durchsuchen (Enter) …")
        onAccepted: Aur.search(text)
    }

    header: Kirigami.InlineMessage {
        position: Kirigami.InlineMessage.Header
        visible: true
        type: Kirigami.MessageType.Warning
        text: i18n("AUR = ungeprüfter Community-Code. Jeder PKGBUILD wird vor dem Bauen angezeigt und muss bestätigt werden.")
    }

    ListView {
        id: list
        model: Aur.model
        clip: true

        delegate: QQC2.ItemDelegate {
            id: row
            width: ListView.view.width
            required property string name
            required property string pkgBase
            required property string version
            required property string description
            required property string maintainer
            required property int votes
            required property bool outOfDate

            contentItem: ColumnLayout {
                spacing: 0
                RowLayout {
                    Layout.fillWidth: true
                    QQC2.Label { text: row.name; font.weight: Font.DemiBold }
                    QQC2.Label { text: row.version; opacity: 0.6; font: Kirigami.Theme.smallFont }
                    Kirigami.Icon {
                        source: "data-warning"
                        visible: row.outOfDate
                        implicitWidth: Kirigami.Units.iconSizes.small
                        implicitHeight: Kirigami.Units.iconSizes.small
                    }
                    Item { Layout.fillWidth: true }
                    QQC2.Label { text: i18n("▲ %1", row.votes); opacity: 0.6; font: Kirigami.Theme.smallFont }
                }
                QQC2.Label {
                    text: row.description
                    opacity: 0.75
                    font: Kirigami.Theme.smallFont
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
            onClicked: applicationWindow().pageStack.push(
                Qt.resolvedUrl("AurDetailPage.qml"),
                { pkgBase: row.pkgBase, appName: row.name, version: row.version, description: row.description })
        }

        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            width: parent.width - Kirigami.Units.gridUnit * 4
            visible: list.count === 0
            icon.name: "applications-other"
            text: i18n("AUR durchsuchen")
            explanation: Aur.status
        }
    }

    footer: Kirigami.InlineMessage {
        visible: Aur.status.length > 0
        position: Kirigami.InlineMessage.Footer
        type: Kirigami.MessageType.Information
        text: Aur.status
    }
}
