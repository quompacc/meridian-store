// Store landing — what you see before searching or picking a category. Reads as
// a store (featured carousel + curated rows per category), not a flat list. The
// flat grid appears only once a category or search narrows the catalog.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

QQC2.ScrollView {
    id: landing

    // Forwarded up to Main so it can push the detail page / set the category.
    signal appActivated(var item)
    signal categorySelected(string key)

    contentWidth: availableWidth
    QQC2.ScrollBar.horizontal.policy: QQC2.ScrollBar.AlwaysOff

    readonly property var categories: [
        { key: "AudioVideo",  label: i18n("Multimedia") },
        { key: "Network",     label: i18n("Internet") },
        { key: "Graphics",    label: i18n("Grafik") },
        { key: "Office",      label: i18n("Büro") },
        { key: "Development", label: i18n("Entwicklung") },
        { key: "Game",        label: i18n("Spiele") },
        { key: "Utility",     label: i18n("Zubehör") },
        { key: "System",      label: i18n("System") },
        { key: "Science",     label: i18n("Wissenschaft") }
    ]

    ColumnLayout {
        width: landing.availableWidth
        spacing: Kirigami.Units.largeSpacing * 2

        // Identity banner: name the platform the store serves.
        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.largeSpacing
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            spacing: 0
            Kirigami.Heading { text: i18n("Software für Arch Linux"); level: 1 }
            QQC2.Label {
                text: i18n("Offizielle Arch-Repos, das kuratierte [meridian]-Repo, Flatpak und AUR — an einem Ort.")
                opacity: 0.7
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }
        }

        AppRow {
            Layout.fillWidth: true
            title: i18n("Empfohlen")
            model: Catalog.featured
            canSeeAll: false
            onAppActivated: (item) => landing.appActivated(item)
        }

        Repeater {
            model: landing.categories
            delegate: AppRow {
                required property var modelData
                Layout.fillWidth: true
                title: modelData.label
                model: Catalog.categoryModel(modelData.key)
                canSeeAll: true
                onAppActivated: (item) => landing.appActivated(item)
                onSeeAll: landing.categorySelected(modelData.key)
            }
        }

        Item { Layout.preferredHeight: Kirigami.Units.largeSpacing }
    }
}
