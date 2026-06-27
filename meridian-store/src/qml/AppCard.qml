// Store-style app card: large icon, name + summary, source badges, install state.
// Used by both the Discover grid and the featured row.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.AbstractCard {
    id: card

    property string appName
    property string appSummary
    property string appIcon
    property string appPkg
    property string appRepo        // sync repo: "core"/"extra"/… or "meridian"
    property string appFlatpakRef
    property bool appInstalled

    readonly property bool hasNative: appPkg.length > 0
    readonly property bool hasFlatpak: appFlatpakRef.length > 0
    readonly property bool isMeridian: appRepo === "meridian"

    showClickFeedback: true

    contentItem: RowLayout {
        spacing: Kirigami.Units.largeSpacing

        Kirigami.Icon {
            source: card.appIcon
            fallback: "package-x-generic"
            Layout.preferredWidth: Kirigami.Units.iconSizes.huge
            Layout.preferredHeight: Kirigami.Units.iconSizes.huge
            Layout.alignment: Qt.AlignVCenter
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: Kirigami.Units.smallSpacing

            QQC2.Label {
                text: card.appName
                font.weight: Font.DemiBold
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            QQC2.Label {
                text: card.appSummary
                opacity: 0.7
                font: Kirigami.Theme.smallFont
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            // Source / status badges.
            RowLayout {
                spacing: Kirigami.Units.smallSpacing
                Layout.fillWidth: true

                // Curated [meridian] lane gets its own, highlighted badge; other
                // sync repos read as the official Arch lane.
                SourceBadge {
                    visible: card.isMeridian
                    label: i18n("[meridian]")
                    accent: Kirigami.Theme.highlightColor
                }
                SourceBadge {
                    visible: card.hasNative && !card.isMeridian
                    label: i18n("Arch")
                    accent: Kirigami.Theme.positiveTextColor
                }
                SourceBadge {
                    visible: card.hasFlatpak
                    label: i18n("Flatpak")
                    accent: Kirigami.Theme.neutralTextColor
                }
                Item { Layout.fillWidth: true }
                RowLayout {
                    spacing: Kirigami.Units.smallSpacing / 2
                    visible: card.appInstalled
                    Kirigami.Icon {
                        source: "emblem-installed"
                        implicitWidth: Kirigami.Units.iconSizes.small
                        implicitHeight: Kirigami.Units.iconSizes.small
                    }
                    QQC2.Label {
                        text: i18n("Installiert")
                        font: Kirigami.Theme.smallFont
                        opacity: 0.7
                    }
                }
            }
        }
    }

    // Small rounded source pill.
    component SourceBadge: Rectangle {
        property string label
        property color accent
        implicitWidth: badgeText.implicitWidth + Kirigami.Units.smallSpacing * 2
        implicitHeight: badgeText.implicitHeight + Kirigami.Units.smallSpacing
        radius: Kirigami.Units.cornerRadius
        color: Qt.rgba(accent.r, accent.g, accent.b, 0.15)
        QQC2.Label {
            id: badgeText
            anchors.centerIn: parent
            text: parent.label
            color: parent.accent
            font: Kirigami.Theme.smallFont
        }
    }
}
