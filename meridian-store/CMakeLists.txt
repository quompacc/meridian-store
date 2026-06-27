cmake_minimum_required(VERSION 3.16)

project(meridian-store VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

# Pin Qt 6 before the KDE modules run: with qt5-base co-installed, ECM otherwise
# leaves QT_MAJOR_VERSION unset here and leaks Qt5 targets into generation.
set(QT_MAJOR_VERSION 6)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Qml Quick QuickControls2 DBus Network)
find_package(KF6 REQUIRED COMPONENTS Kirigami I18n StatusNotifierItem)
find_package(AppStreamQt REQUIRED)

# libalpm has no CMake/pkg-config module on Arch; resolve by hand.
find_library(ALPM_LIBRARY NAMES alpm REQUIRED)
find_path(ALPM_INCLUDE_DIR NAMES alpm.h REQUIRED)

qt_standard_project_setup(REQUIRES 6.5)

qt_add_executable(meridian-store
    src/main.cpp
    src/catalog.cpp
    src/alpmdb.cpp
    src/storedaemon.cpp
    src/flatpakbackend.cpp
    src/aurbackend.cpp
    src/updates.cpp
    src/trayicon.cpp
)

qt_add_qml_module(meridian-store
    URI org.meridian.store
    VERSION 1.0
    QML_FILES
        src/qml/Main.qml
        src/qml/AppCard.qml
        src/qml/AppTile.qml
        src/qml/AppRow.qml
        src/qml/DiscoverLanding.qml
        src/qml/AppDetailPage.qml
        src/qml/ScreenshotGallery.qml
        src/qml/UpdatesPage.qml
        src/qml/AurPage.qml
        src/qml/AurDetailPage.qml
        src/qml/SettingsPage.qml
        src/qml/AboutPage.qml
)

target_include_directories(meridian-store PRIVATE ${ALPM_INCLUDE_DIR})

target_compile_definitions(meridian-store PRIVATE
    MERIDIAN_STORE_VERSION="${PROJECT_VERSION}")

target_link_libraries(meridian-store PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::DBus
    Qt6::Network
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    KF6::Kirigami
    KF6::I18n
    KF6::I18nQml
    KF6::StatusNotifierItem
    AppStreamQt
    ${ALPM_LIBRARY}
)

install(TARGETS meridian-store ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
