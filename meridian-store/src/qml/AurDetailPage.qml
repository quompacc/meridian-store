// AUR-Detail/Review-Seite (Phase 4): zeigt den PKGBUILD; Bauen ist erst nach
// expliziter Bestätigung möglich. Der Build läuft im Daemon in sauberem Chroot.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: page

    property string pkgBase
    property string appName
    property string version
    property string description
    property string pkgbuild: ""
    property string reviewRev: ""   // commit the PKGBUILD was fetched at; build is pinned to it
    property bool reviewed: false

    title: appName

    Component.onCompleted: Aur.fetchPkgbuild(pkgBase)

    Connections {
        target: Aur
        function onPkgbuildFetched(pb, rev, text) {
            if (pb === page.pkgBase) {
                page.reviewRev = rev;
                page.pkgbuild = text;
            }
        }
    }

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        ColumnLayout {
            spacing: 0
            Kirigami.Heading { text: page.appName + "  " + page.version; level: 1 }
            QQC2.Label { text: page.description; opacity: 0.8; wrapMode: Text.WordWrap; Layout.fillWidth: true }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: true
            type: Kirigami.MessageType.Warning
            text: i18n("AUR-Paket — nicht von Arch Linux geprüft. PKGBUILD und mitgelieferte Dateien (z. B. *.install) werden beim Bauen ausgeführt. Der Build wird exakt auf die unten geprüfte Revision festgenagelt; ändert sich das Repo danach, bricht der Build ab.")
        }

        // The exact commit the shown PKGBUILD belongs to — the build is pinned here.
        QQC2.Label {
            visible: page.reviewRev.length > 0
            text: i18n("Revision: %1", page.reviewRev.substring(0, 12))
            font.family: "monospace"
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            opacity: 0.7
        }

        QQC2.Frame {
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 16
            QQC2.ScrollView {
                anchors.fill: parent
                QQC2.TextArea {
                    text: page.pkgbuild.length > 0 ? page.pkgbuild : i18n("# PKGBUILD wird geladen …")
                    readOnly: true
                    wrapMode: TextEdit.NoWrap
                    font.family: "monospace"
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                }
            }
        }

        QQC2.CheckBox {
            id: reviewBox
            text: i18n("Ich habe den PKGBUILD geprüft und vertraue ihm")
            enabled: page.pkgbuild.length > 0 && page.reviewRev.length > 0
            onToggled: page.reviewed = checked
        }

        RowLayout {
            Layout.fillWidth: true
            QQC2.Button {
                text: i18n("Bauen & installieren")
                icon.name: "run-build"
                enabled: Daemon.available && !Daemon.busy && page.reviewed && page.reviewRev.length > 0
                onClicked: {
                    Daemon.clearLog();
                    Daemon.buildAur(page.pkgBase, page.reviewRev);
                }
            }
            QQC2.BusyIndicator {
                running: Daemon.busy
                visible: Daemon.busy
                implicitWidth: Kirigami.Units.iconSizes.medium
                implicitHeight: Kirigami.Units.iconSizes.medium
            }
            Item { Layout.fillWidth: true }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: !Daemon.available
            type: Kirigami.MessageType.Warning
            text: i18n("Der Transaktions-Daemon ist nicht installiert — Bauen deaktiviert.")
        }

        QQC2.Frame {
            id: logFrame
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 12
            visible: Daemon.log.length > 0
            QQC2.ScrollView {
                anchors.fill: parent
                QQC2.TextArea {
                    text: Daemon.log
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
