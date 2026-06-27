// Updates: lists pending repo + Flatpak updates. Applying is delegated —
// repo via the polkit daemon (UpdateSystem / Refresh), Flatpak via its backend.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: page

    title: i18nc("@title", "Aktualisierungen")

    readonly property bool anyBusy: Daemon.busy || Flatpak.busy || Updates.checking

    Component.onCompleted: Updates.check()

    Connections {
        target: Daemon
        // After a DB refresh or a system upgrade, re-scan for what's left.
        function onFinished(success, message) { Updates.check(); }
    }
    Connections {
        target: Flatpak
        function onFinished(success, message) { Updates.check(); }
    }

    actions: [
        Kirigami.Action {
            text: i18n("Erneut prüfen")
            icon.name: "view-refresh"
            enabled: !page.anyBusy
            onTriggered: Updates.check()
        },
        Kirigami.Action {
            text: i18n("System aktualisieren")
            icon.name: "system-software-update"
            enabled: Daemon.available && !page.anyBusy && Updates.repoCount > 0
            onTriggered: { Daemon.clearLog(); Daemon.updateSystem(); }
        },
        Kirigami.Action {
            text: i18n("Flatpaks aktualisieren")
            icon.name: "flatpak"
            enabled: Flatpak.available && !page.anyBusy && Updates.flatpakCount > 0
            onTriggered: { Flatpak.clearLog(); Flatpak.updateAll(); }
        }
    ]

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: true
            type: Updates.count > 0 ? Kirigami.MessageType.Information : Kirigami.MessageType.Positive
            text: Updates.checking
                ? i18n("Suche nach Aktualisierungen …")
                : (Updates.count > 0
                    ? i18np("%1 Aktualisierung verfügbar", "%1 Aktualisierungen verfügbar", Updates.count)
                    : i18n("Alles aktuell"))
        }

        Repeater {
            model: Updates.model
            delegate: Kirigami.AbstractCard {
                required property string name
                required property string oldVersion
                required property string newVersion
                required property string source

                Layout.fillWidth: true
                contentItem: RowLayout {
                    spacing: Kirigami.Units.largeSpacing
                    Kirigami.Icon {
                        source: source === "flatpak" ? "flatpak" : "package-x-generic"
                        fallback: "package-x-generic"
                        implicitWidth: Kirigami.Units.iconSizes.medium
                        implicitHeight: width
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 0
                        QQC2.Label { text: name; font.weight: Font.DemiBold; elide: Text.ElideRight; Layout.fillWidth: true }
                        QQC2.Label {
                            visible: oldVersion.length > 0
                            text: oldVersion + " → " + newVersion
                            font: Kirigami.Theme.smallFont; opacity: 0.7
                        }
                    }
                    QQC2.Label {
                        text: source === "flatpak" ? i18n("Flatpak") : i18n("Repo")
                        font: Kirigami.Theme.smallFont
                        opacity: 0.6
                    }
                }
            }
        }

        QQC2.BusyIndicator {
            running: page.anyBusy
            visible: running
            Layout.alignment: Qt.AlignHCenter
        }

        // Live log of the active transaction.
        QQC2.Frame {
            id: logFrame
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 12
            readonly property string activeLog: Flatpak.busy ? Flatpak.log
                : (Daemon.busy || Daemon.log.length > 0 ? Daemon.log : Flatpak.log)
            visible: activeLog.length > 0

            QQC2.ScrollView {
                anchors.fill: parent
                QQC2.TextArea {
                    text: logFrame.activeLog
                    readOnly: true
                    wrapMode: TextEdit.NoWrap
                    font.family: "monospace"
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    onTextChanged: cursorPosition = length
                }
            }
        }
    }
}
