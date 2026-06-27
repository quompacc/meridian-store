// MeridianOS Software-Store. Store-style landing (featured + per-category rows);
// the flat grid appears only under a category or search. Destinations (detail,
// updates, settings, AUR) replace the single detail column instead of stacking.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: root

    title: i18nc("@title:window", "Meridian Store")
    width: Kirigami.Units.gridUnit * 52
    height: Kirigami.Units.gridUnit * 38
    minimumWidth: Kirigami.Units.gridUnit * 32
    minimumHeight: Kirigami.Units.gridUnit * 24

    // Launched with --background it starts in the tray; the tray brings it up.
    visible: !startHidden

    function raiseWindow() {
        root.show();
        root.raise();
        root.requestActivate();
    }

    Connections {
        target: Tray
        function onActivated() { root.raiseWindow(); }
        function onShowUpdates() {
            root.raiseWindow();
            root.navigate(Qt.resolvedUrl("UpdatesPage.qml"));
        }
    }

    // Open a destination page as the *single* secondary column: collapse anything
    // already pushed back to the landing first, so repeated clicks never stack
    // up new columns (and a second click on the same entry just refreshes it).
    function navigate(url, props) {
        while (pageStack.depth > 1) {
            pageStack.pop();
        }
        return props ? pageStack.push(url, props) : pageStack.push(url);
    }

    function openApp(item) {
        root.navigate(Qt.resolvedUrl("AppDetailPage.qml"), {
            appId: item.appId,
            appName: item.name,
            appSummary: item.summary,
            appIcon: item.icon,
            appPkg: item.pkgName,
            appFlatpakRef: item.flatpakRef
        });
    }

    // Freedesktop-Hauptkategorien → AppStream-Schlüssel.
    readonly property var categoryList: [
        { key: "",            label: i18n("Alle Apps"),    icon: "applications-all" },
        { key: "AudioVideo",  label: i18n("Multimedia"),   icon: "applications-multimedia" },
        { key: "Development", label: i18n("Entwicklung"),  icon: "applications-development" },
        { key: "Game",        label: i18n("Spiele"),       icon: "applications-games" },
        { key: "Graphics",    label: i18n("Grafik"),       icon: "applications-graphics" },
        { key: "Network",     label: i18n("Internet"),     icon: "applications-internet" },
        { key: "Office",      label: i18n("Büro"),         icon: "applications-office" },
        { key: "Science",     label: i18n("Wissenschaft"), icon: "applications-science" },
        { key: "System",      label: i18n("System"),       icon: "applications-system" },
        { key: "Utility",     label: i18n("Zubehör"),      icon: "applications-utilities" }
    ]

    globalDrawer: Kirigami.GlobalDrawer {
        isMenu: false
        modal: false
        width: Kirigami.Units.gridUnit * 13
        header: Kirigami.AbstractApplicationHeader {
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Kirigami.Units.largeSpacing
                Kirigami.Icon { source: "applications-all"; implicitWidth: Kirigami.Units.iconSizes.smallMedium; implicitHeight: width }
                Kirigami.Heading { text: i18n("Kategorien"); level: 4; Layout.fillWidth: true }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0
            Repeater {
                model: root.categoryList
                delegate: QQC2.ItemDelegate {
                    required property var modelData
                    Layout.fillWidth: true
                    highlighted: Catalog.category === modelData.key
                    contentItem: RowLayout {
                        spacing: Kirigami.Units.largeSpacing
                        Kirigami.Icon { source: modelData.icon; implicitWidth: Kirigami.Units.iconSizes.smallMedium; implicitHeight: width }
                        QQC2.Label { text: modelData.label; Layout.fillWidth: true; elide: Text.ElideRight }
                    }
                    onClicked: {
                        // Return to the landing/grid column, then filter.
                        while (root.pageStack.depth > 1) root.pageStack.pop();
                        Catalog.category = modelData.key;
                    }
                }
            }

            Kirigami.Separator { Layout.fillWidth: true }

            // Updates — with a live count badge.
            QQC2.ItemDelegate {
                Layout.fillWidth: true
                contentItem: RowLayout {
                    spacing: Kirigami.Units.largeSpacing
                    Kirigami.Icon { source: "system-software-update"; implicitWidth: Kirigami.Units.iconSizes.smallMedium; implicitHeight: width }
                    QQC2.Label { text: i18n("Aktualisierungen"); Layout.fillWidth: true; elide: Text.ElideRight }
                    Rectangle {
                        visible: Updates.count > 0
                        radius: height / 2
                        color: Kirigami.Theme.highlightColor
                        implicitHeight: countLabel.implicitHeight + Kirigami.Units.smallSpacing / 2
                        implicitWidth: Math.max(implicitHeight, countLabel.implicitWidth + Kirigami.Units.smallSpacing)
                        QQC2.Label {
                            id: countLabel
                            anchors.centerIn: parent
                            text: Updates.count
                            color: Kirigami.Theme.highlightedTextColor
                            font: Kirigami.Theme.smallFont
                        }
                    }
                }
                onClicked: root.navigate(Qt.resolvedUrl("UpdatesPage.qml"))
            }

            // Die ungeprüfte AUR-Spur — nur sichtbar, wenn bewusst aktiviert.
            QQC2.ItemDelegate {
                Layout.fillWidth: true
                visible: Aur.enabled
                contentItem: RowLayout {
                    spacing: Kirigami.Units.largeSpacing
                    Kirigami.Icon { source: "applications-other"; implicitWidth: Kirigami.Units.iconSizes.smallMedium; implicitHeight: width }
                    QQC2.Label { text: i18n("AUR (ungeprüft)"); Layout.fillWidth: true; elide: Text.ElideRight }
                }
                onClicked: root.navigate(Qt.resolvedUrl("AurPage.qml"))
            }

            QQC2.ItemDelegate {
                Layout.fillWidth: true
                contentItem: RowLayout {
                    spacing: Kirigami.Units.largeSpacing
                    Kirigami.Icon { source: "settings-configure"; implicitWidth: Kirigami.Units.iconSizes.smallMedium; implicitHeight: width }
                    QQC2.Label { text: i18n("Einstellungen"); Layout.fillWidth: true; elide: Text.ElideRight }
                }
                onClicked: root.navigate(Qt.resolvedUrl("SettingsPage.qml"))
            }

            QQC2.ItemDelegate {
                Layout.fillWidth: true
                contentItem: RowLayout {
                    spacing: Kirigami.Units.largeSpacing
                    Kirigami.Icon { source: "help-about"; implicitWidth: Kirigami.Units.iconSizes.smallMedium; implicitHeight: width }
                    QQC2.Label { text: i18n("Über"); Layout.fillWidth: true; elide: Text.ElideRight }
                }
                onClicked: root.navigate(Qt.resolvedUrl("AboutPage.qml"))
            }
        }
    }

    pageStack.initialPage: Kirigami.Page {
        id: discoverPage
        title: i18nc("@title", "Entdecken")
        padding: 0

        readonly property bool landingMode: Catalog.category === "" && Catalog.search === ""

        titleDelegate: Kirigami.SearchField {
            Layout.fillWidth: true
            Layout.maximumWidth: Kirigami.Units.gridUnit * 24
            placeholderText: i18n("Apps durchsuchen …")
            onTextChanged: Catalog.search = text
        }

        // --- Store landing (unfiltered) ---
        DiscoverLanding {
            anchors.fill: parent
            visible: discoverPage.landingMode
            onAppActivated: (item) => root.openApp(item)
            onCategorySelected: (key) => Catalog.category = key
        }

        // --- Flat grid (category or search active) ---
        GridView {
            id: grid
            anchors.fill: parent
            visible: !discoverPage.landingMode
            model: Catalog.model
            clip: true
            cacheBuffer: Kirigami.Units.gridUnit * 40

            readonly property int targetWidth: Kirigami.Units.gridUnit * 22
            readonly property int columns: Math.max(1, Math.floor(width / targetWidth))
            cellWidth: Math.floor(width / columns)
            cellHeight: Kirigami.Units.gridUnit * 6

            delegate: Item {
                id: cell
                width: grid.cellWidth
                height: grid.cellHeight

                required property string name
                required property string summary
                required property string icon
                required property string appId
                required property string pkgName
                required property string repo
                required property string flatpakRef
                required property bool installed

                AppCard {
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.smallSpacing
                    appName: cell.name
                    appSummary: cell.summary
                    appIcon: cell.icon
                    appPkg: cell.pkgName
                    appRepo: cell.repo
                    appFlatpakRef: cell.flatpakRef
                    appInstalled: cell.installed
                    onClicked: root.openApp({
                        appId: cell.appId, name: cell.name, summary: cell.summary,
                        icon: cell.icon, pkgName: cell.pkgName, flatpakRef: cell.flatpakRef
                    })
                }
            }

            Kirigami.PlaceholderMessage {
                anchors.centerIn: parent
                width: parent.width - Kirigami.Units.gridUnit * 4
                visible: grid.count === 0
                icon.name: Catalog.loaded ? "search" : "view-refresh"
                text: Catalog.loaded ? i18n("Keine Treffer") : i18n("Katalog wird geladen …")
                explanation: Catalog.loaded ? i18n("Andere Kategorie oder Suchbegriff probieren.") : ""
            }
        }

        footer: Kirigami.InlineMessage {
            visible: !discoverPage.landingMode
            position: Kirigami.InlineMessage.Footer
            type: Kirigami.MessageType.Information
            text: i18np("%1 App", "%1 Apps", Catalog.count)
        }
    }
}
