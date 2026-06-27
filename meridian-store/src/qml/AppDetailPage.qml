// App detail page: rich metadata (screenshots, description, version, license,
// developer, homepage) + multi-source actions. Repo packages run through the
// polkit daemon, Flatpaks through the Flatpak backend.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: page

    property string appId
    property string appName
    property string appSummary
    property string appIcon
    property string appPkg
    property string appFlatpakRef

    // Per-source live state (on page build and after every transaction).
    property bool nativeInstalled: false
    property bool flatpakInstalled: false

    // Rich metadata, fetched once from the AppStream pool.
    property var info: ({})

    readonly property bool hasNative: appPkg.length > 0
    readonly property bool hasFlatpak: appFlatpakRef.length > 0 && Flatpak.enabled

    // Primary action for the prominent header button. Prefers the repo source
    // (the per-source frames below still expose the other one explicitly).
    readonly property bool primaryInstalled: hasNative ? nativeInstalled : flatpakInstalled
    readonly property bool primaryEnabled: hasNative
        ? (Daemon.available && !Daemon.busy && !Flatpak.busy)
        : (Flatpak.available && !Flatpak.busy && !Daemon.busy)

    function primaryAction() {
        if (hasNative) {
            Daemon.clearLog();
            nativeInstalled ? Daemon.remove([appPkg]) : Daemon.install([appPkg]);
        } else if (hasFlatpak) {
            Flatpak.clearLog();
            flatpakInstalled ? Flatpak.remove(appFlatpakRef) : Flatpak.install(appFlatpakRef);
        }
    }

    title: appName

    function refreshState() {
        nativeInstalled = hasNative ? Catalog.isPkgInstalled(appPkg) : false;
        flatpakInstalled = hasFlatpak ? Flatpak.isInstalled(appFlatpakRef) : false;
    }

    Component.onCompleted: {
        info = Catalog.details(appId);
        refreshState();
    }

    Connections {
        target: Daemon
        function onFinished(success, message) { if (success) page.refreshState(); }
    }
    Connections {
        target: Flatpak
        function onFinished(success, message) { if (success) page.refreshState(); }
    }

    ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        // --- Header: icon, name, summary, quick facts ---
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing

            Kirigami.Icon {
                source: page.appIcon
                fallback: "package-x-generic"
                Layout.preferredWidth: Kirigami.Units.iconSizes.huge
                Layout.preferredHeight: Kirigami.Units.iconSizes.huge
                Layout.alignment: Qt.AlignTop
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing
                Kirigami.Heading { text: page.appName; level: 1 }
                QQC2.Label {
                    text: page.appSummary
                    opacity: 0.8
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
                RowLayout {
                    spacing: Kirigami.Units.largeSpacing
                    Layout.topMargin: Kirigami.Units.smallSpacing
                    QQC2.Label {
                        visible: !!page.info.developer
                        text: i18n("von %1", page.info.developer || "")
                        font: Kirigami.Theme.smallFont
                        opacity: 0.7
                    }
                    QQC2.Label {
                        visible: !!page.info.version
                        text: page.info.version + (page.info.releaseDate ? " · " + page.info.releaseDate : "")
                        font: Kirigami.Theme.smallFont
                        opacity: 0.7
                    }
                }
            }

            // Prominent primary action, app-store style: top-right of the header.
            QQC2.Button {
                visible: page.hasNative || page.hasFlatpak
                Layout.alignment: Qt.AlignTop
                Layout.preferredHeight: Kirigami.Units.gridUnit * 2.2
                Layout.minimumWidth: Kirigami.Units.gridUnit * 8
                text: page.primaryInstalled ? i18n("Deinstallieren") : i18n("Installieren")
                icon.name: page.primaryInstalled ? "edit-delete" : "install"
                enabled: page.primaryEnabled
                // Make the install case stand out; deinstall stays neutral.
                highlighted: !page.primaryInstalled
                font.weight: Font.DemiBold
                onClicked: page.primaryAction()
            }
        }

        // --- Screenshots ---
        ScreenshotGallery {
            Layout.fillWidth: true
            urls: page.info.screenshots || []
        }

        // --- Long description ---
        QQC2.Label {
            visible: !!page.info.description
            text: page.info.description || ""
            textFormat: Text.RichText
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
            // Only follow web links — never hand an arbitrary scheme from
            // (external) AppStream metadata to the system URL opener.
            onLinkActivated: (link) => {
                if (/^https?:\/\//i.test(link))
                    Qt.openUrlExternally(link);
            }
        }

        // --- Metadata chips ---
        Flow {
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing
            visible: !!page.info.license || !!page.info.homepage

            QQC2.Label {
                visible: !!page.info.license
                text: i18n("Lizenz: %1", page.info.license || "")
                font: Kirigami.Theme.smallFont
                opacity: 0.7
            }
            Kirigami.UrlButton {
                visible: !!page.info.homepage
                url: page.info.homepage || ""
                text: i18n("Webseite")
            }
        }

        Kirigami.Separator { Layout.fillWidth: true }

        Kirigami.Heading { text: i18n("Quellen"); level: 4 }

        // --- Repo (pacman, via the polkit daemon) ---
        QQC2.Frame {
            Layout.fillWidth: true
            visible: page.hasNative
            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing
                Kirigami.Icon { source: "package-x-generic"; implicitWidth: Kirigami.Units.iconSizes.medium; implicitHeight: width }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    QQC2.Label {
                        text: page.info.repo === "meridian"
                            ? i18n("[meridian] — kuratiert & signiert")
                            : (page.info.repo
                                ? i18n("Offizielle Arch-Repos · %1", page.info.repo)
                                : i18n("Offizielles Repo (Arch / [meridian])"))
                        font.weight: Font.DemiBold
                    }
                    QQC2.Label {
                        text: page.appPkg + " · " + (page.nativeInstalled ? i18n("installiert") : i18n("verfügbar"))
                        font: Kirigami.Theme.smallFont; opacity: 0.7
                    }
                }
                QQC2.Button {
                    text: page.nativeInstalled ? i18n("Deinstallieren") : i18n("Installieren")
                    icon.name: page.nativeInstalled ? "edit-delete" : "install"
                    enabled: Daemon.available && !Daemon.busy && !Flatpak.busy
                    onClicked: {
                        Daemon.clearLog();
                        page.nativeInstalled ? Daemon.remove([page.appPkg]) : Daemon.install([page.appPkg]);
                    }
                }
            }
        }

        // --- Flatpak (Flathub, via Flatpak's own polkit integration) ---
        QQC2.Frame {
            Layout.fillWidth: true
            visible: page.hasFlatpak
            contentItem: RowLayout {
                spacing: Kirigami.Units.largeSpacing
                Kirigami.Icon { source: "flatpak"; fallback: "application-x-executable"; implicitWidth: Kirigami.Units.iconSizes.medium; implicitHeight: width }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    QQC2.Label { text: i18n("Flatpak (Flathub)"); font.weight: Font.DemiBold }
                    QQC2.Label {
                        text: page.appFlatpakRef + " · " + (page.flatpakInstalled ? i18n("installiert") : i18n("verfügbar"))
                        font: Kirigami.Theme.smallFont; opacity: 0.7
                    }
                }
                QQC2.Button {
                    text: page.flatpakInstalled ? i18n("Deinstallieren") : i18n("Installieren")
                    icon.name: page.flatpakInstalled ? "edit-delete" : "install"
                    enabled: Flatpak.available && !Flatpak.busy && !Daemon.busy
                    onClicked: {
                        Flatpak.clearLog();
                        page.flatpakInstalled ? Flatpak.remove(page.appFlatpakRef) : Flatpak.install(page.appFlatpakRef);
                    }
                }
            }
        }

        QQC2.BusyIndicator {
            running: Daemon.busy || Flatpak.busy
            visible: running
            Layout.alignment: Qt.AlignHCenter
        }

        // Shared live log of the most recent/active source.
        QQC2.Frame {
            id: logFrame
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 12
            readonly property string activeLog: Flatpak.busy ? Flatpak.log
                : (Daemon.busy ? Daemon.log
                : (Flatpak.log.length > 0 ? Flatpak.log : Daemon.log))
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
