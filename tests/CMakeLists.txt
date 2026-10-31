add_executable(tst_nmclient tst_nmclient.cpp)
target_link_libraries(tst_nmclient PRIVATE wgtray_core Qt6::Test)
add_test(NAME nmclient COMMAND tst_nmclient)

add_executable(tst_confparser tst_confparser.cpp)
target_link_libraries(tst_confparser PRIVATE wgtray_core Qt6::Test)
add_test(NAME confparser COMMAND tst_confparser)

add_executable(tst_tunneldetails tst_tunneldetails.cpp)
target_link_libraries(tst_tunneldetails PRIVATE wgtray_core Qt6::Test)
add_test(NAME tunneldetails COMMAND tst_tunneldetails)

# Needs python3 with dbus-python + PyGObject, and dbus-run-session; skipped otherwise.
add_executable(tst_nmintegration tst_nmintegration.cpp)
target_link_libraries(tst_nmintegration PRIVATE wgtray_core Qt6::Test)
find_program(DBUS_RUN_SESSION dbus-run-session)
if(DBUS_RUN_SESSION)
    add_test(NAME nmintegration
        COMMAND ${DBUS_RUN_SESSION} -- ${CMAKE_CURRENT_SOURCE_DIR}/run_integration.sh
                ${CMAKE_CURRENT_SOURCE_DIR}/fake_nm.py $<TARGET_FILE:tst_nmintegration>)
endif()

add_executable(tst_aliasstore tst_aliasstore.cpp)
target_link_libraries(tst_aliasstore PRIVATE wgtray_core Qt6::Test)
add_test(NAME aliasstore COMMAND tst_aliasstore)

add_executable(tst_preferences tst_preferences.cpp)
target_link_libraries(tst_preferences PRIVATE wgtray_core Qt6::Test)
add_test(NAME preferences COMMAND tst_preferences)

add_executable(tst_netstats tst_netstats.cpp)
target_link_libraries(tst_netstats PRIVATE wgtray_core Qt6::Test)
add_test(NAME netstats COMMAND tst_netstats)

add_executable(tst_manualconfig tst_manualconfig.cpp)
target_link_libraries(tst_manualconfig PRIVATE wgtray_core Qt6::Test)
add_test(NAME manualconfig COMMAND tst_manualconfig)

# The dialog is a widget: needs a QApplication, so drive it on the offscreen platform.
add_executable(tst_newconnectiondialog tst_newconnectiondialog.cpp
    ../src/ui/NewConnectionDialog.cpp ../src/ui/NewConnectionDialog.h)
target_include_directories(tst_newconnectiondialog PRIVATE ../src)
target_link_libraries(tst_newconnectiondialog PRIVATE wgtray_core Qt6::Widgets Qt6::Test)
add_test(NAME newconnectiondialog COMMAND tst_newconnectiondialog)
set_tests_properties(newconnectiondialog PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_tray tst_tray.cpp ../src/ui/TrayController.cpp ../src/ui/TrayController.h
    ../src/ui/resources/resources.qrc)
target_include_directories(tst_tray PRIVATE ../src)
target_link_libraries(tst_tray PRIVATE wgtray_core Qt6::Widgets Qt6::Test)
add_test(NAME tray COMMAND tst_tray)
set_tests_properties(tray PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_export tst_export.cpp)
target_link_libraries(tst_export PRIVATE wgtray_core Qt6::Test)
add_test(NAME export COMMAND tst_export)
set_tests_properties(export PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

add_executable(tst_diagnostics tst_diagnostics.cpp)
target_link_libraries(tst_diagnostics PRIVATE wgtray_core Qt6::Test)
add_test(NAME diagnostics COMMAND tst_diagnostics)

add_executable(tst_troubleshootdialog tst_troubleshootdialog.cpp
    ../src/ui/TroubleshootDialog.cpp ../src/ui/TroubleshootDialog.h)
target_include_directories(tst_troubleshootdialog PRIVATE ../src)
target_link_libraries(tst_troubleshootdialog PRIVATE wgtray_core Qt6::Widgets Qt6::Test)
add_test(NAME troubleshootdialog COMMAND tst_troubleshootdialog)
set_tests_properties(troubleshootdialog PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
