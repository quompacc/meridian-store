// Horizontal screenshot strip with click-to-enlarge. Fed a list of image URLs.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: gallery

    property var urls: []

    visible: urls.length > 0
    spacing: Kirigami.Units.largeSpacing

    Kirigami.Heading { text: i18n("Screenshots"); level: 4 }

    QQC2.ScrollView {
        Layout.fillWidth: true
        Layout.preferredHeight: Kirigami.Units.gridUnit * 16
        QQC2.ScrollBar.vertical.policy: QQC2.ScrollBar.AlwaysOff

        ListView {
            id: strip
            orientation: ListView.Horizontal
            spacing: Kirigami.Units.largeSpacing
            clip: true
            model: gallery.urls

            delegate: Rectangle {
                required property string modelData
                height: strip.height
                width: Math.max(Kirigami.Units.gridUnit * 8, shot.paintedWidth)
                radius: Kirigami.Units.cornerRadius
                color: Kirigami.Theme.alternateBackgroundColor

                Image {
                    id: shot
                    anchors.fill: parent
                    anchors.margins: 1
                    source: parent.modelData
                    fillMode: Image.PreserveAspectFit
                    asynchronous: true
                    cache: true
                }
                QQC2.BusyIndicator {
                    anchors.centerIn: parent
                    running: shot.status === Image.Loading
                }
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        viewer.source = parent.modelData;
                        viewer.open();
                    }
                }
            }
        }
    }

    // Full-window enlarged view.
    QQC2.Popup {
        id: viewer
        property alias source: bigShot.source
        parent: QQC2.Overlay.overlay
        anchors.centerIn: parent
        modal: true
        width: parent ? parent.width * 0.9 : 0
        height: parent ? parent.height * 0.9 : 0
        padding: 0

        Image {
            id: bigShot
            anchors.fill: parent
            fillMode: Image.PreserveAspectFit
            asynchronous: true
        }
        MouseArea {
            anchors.fill: parent
            onClicked: viewer.close()
        }
    }
}
