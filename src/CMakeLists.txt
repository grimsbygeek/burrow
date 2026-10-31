add_library(wgtray_core STATIC
    nm/ActiveWatcher.h
    nm/NmClient.cpp
    nm/NmClient.h
    nm/Tunnel.h
    core/AliasStore.cpp
    core/AliasStore.h
    core/Diagnostics.cpp
    core/Diagnostics.h
    core/ConfWriter.cpp
    core/ConfWriter.h
    core/QrCode.cpp
    core/QrCode.h
    core/KeyGen.cpp
    core/KeyGen.h
    core/ManualConfig.cpp
    core/ManualConfig.h
    core/NetStats.cpp
    core/NetStats.h
    core/Preferences.cpp
    core/Preferences.h
    core/WgConfig.h
    core/ConfParser.cpp
    core/ConfParser.h
    core/NmSettingsMapper.cpp
    core/NmSettingsMapper.h
    core/TunnelDetails.cpp
    core/TunnelDetails.h
)
target_include_directories(wgtray_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
find_package(PkgConfig REQUIRED)
pkg_check_modules(SODIUM REQUIRED IMPORTED_TARGET libsodium)
pkg_check_modules(QRENCODE REQUIRED IMPORTED_TARGET libqrencode)
target_link_libraries(wgtray_core PUBLIC Qt6::Core Qt6::Gui Qt6::DBus Qt6::Network PkgConfig::SODIUM PkgConfig::QRENCODE)
target_compile_options(wgtray_core PUBLIC -Wall -Wextra)

add_executable(wgtray
    main.cpp
    ui/MainWindow.cpp
    ui/MainWindow.h
    ui/ExportDialog.cpp
    ui/ExportDialog.h
    ui/MapBackground.cpp
    ui/NewConnectionDialog.cpp
    ui/NewConnectionDialog.h
    ui/PreferencesDialog.cpp
    ui/PreferencesDialog.h
    ui/Theme.cpp
    ui/Theme.h
    ui/MapBackground.h
    ui/resources/resources.qrc
    ui/TroubleshootDialog.cpp
    ui/TroubleshootDialog.h
    ui/TrayController.cpp
    ui/TrayController.h
    ui/TunnelRow.cpp
    ui/TunnelRow.h
    ui/DetailPane.cpp
    ui/DetailPane.h
)
target_link_libraries(wgtray PRIVATE wgtray_core Qt6::Widgets)
install(TARGETS wgtray RUNTIME DESTINATION bin)

# Desktop entry, AppStream metadata and icons, all named after the application ID
configure_file(${CMAKE_SOURCE_DIR}/packaging/wgtray.desktop.in ${WGTRAY_APP_ID}.desktop @ONLY)
configure_file(${CMAKE_SOURCE_DIR}/packaging/wgtray.metainfo.xml.in ${WGTRAY_APP_ID}.metainfo.xml @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${WGTRAY_APP_ID}.desktop DESTINATION share/applications)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${WGTRAY_APP_ID}.metainfo.xml DESTINATION share/metainfo)
install(FILES ui/resources/icons/wgtray.svg DESTINATION share/icons/hicolor/scalable/apps
        RENAME ${WGTRAY_APP_ID}.svg)
foreach(size 16 24 32 48 64 128 256)
    install(FILES ui/resources/icons/wgtray-${size}.png
            DESTINATION share/icons/hicolor/${size}x${size}/apps RENAME ${WGTRAY_APP_ID}.png)
endforeach()
