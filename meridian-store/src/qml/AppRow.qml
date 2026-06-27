// A titled, horizontally-scrollable strip of app tiles — the building block of
// the store landing. Has a visible horizontal scrollbar and maps the mouse wheel
// to horizontal scrolling, so content beyond the viewport is reachable (instead
// of only appearing when the window is widened).
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: row

    property string title
    property var model
    property bool canSeeAll: false

    signal appActivated(var item)
    signal seeAll()

    spacing: Kirigami.Units.smallSpacing
    visible: list.count > 0

    RowLayout {
        Layout.fillWidth: true
        Layout.leftMargin: Kirigami.Units.largeSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing

        Kirigami.Heading { text: row.title; level: 3 }
        Item { Layout.fillWidth: true }
        QQC2.ToolButton {
            visible: row.canSeeAll
            text: i18n("Alle anzeigen")
            icon.name: "arrow-right"
            onClicked: row.seeAll()
        }
    }

    QQC2.ScrollView {
        Layout.fillWidth: true
        Layout.preferredHeight: Kirigami.Units.gridUnit * 9
        Layout.leftMargin: Kirigami.Units.largeSpacing
        Layout.rightMargin: Kirigami.Units.largeSpacing
        QQC2.ScrollBar.vertical.policy: QQC2.ScrollBar.AlwaysOff
        QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AsNeeded

        ListView {
            id: list
            orientation: ListView.Horizontal
            spacing: Kirigami.Units.largeSpacing
            clip: true
            reuseItems: true
            model: row.model
            boundsBehavior: Flickable.StopAtBounds

            // Vertical wheel → horizontal pan, so a mouse can browse the row.
            WheelHandler {
                acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
                onWheel: (event) => {
                    const dy = event.angleDelta.y !== 0 ? event.angleDelta.y : event.angleDelta.x;
                    list.contentX = Math.max(0,
                        Math.min(list.contentWidth - list.width, list.contentX - dy));
                }
            }

            delegate: Item {
                id: cell
                required property string name
                required property string icon
                required property string appId
                required property string pkgName
                required property string repo
                required property string flatpakRef
                required property bool installed
                required property string summary

                width: Kirigami.Units.gridUnit * 9
                height: list.height

                AppTile {
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.smallSpacing
                    appName: cell.name
                    appIcon: cell.icon
                    appPkg: cell.pkgName
                    appRepo: cell.repo
                    appFlatpakRef: cell.flatpakRef
                    appInstalled: cell.installed
                    onClicked: row.appActivated({
                        appId: cell.appId, name: cell.name, summary: cell.summary,
                        icon: cell.icon, pkgName: cell.pkgName, flatpakRef: cell.flatpakRef
                    })
                }
            }
        }
    }
}
