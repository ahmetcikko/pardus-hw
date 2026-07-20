cmake_minimum_required(VERSION 3.30)
project(HWControlCenter VERSION 1.0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Quick Qml QuickControls2 LinguistTools)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SYSTEMD REQUIRED libsystemd)
qt_standard_project_setup(REQUIRES 6.5)
add_executable(HWControlCenter app/src/main.cpp app/src/backend.h app/src/backend.cpp)
add_executable(PardusOptimizer daemon/main.cpp)
target_include_directories(PardusOptimizer PRIVATE ${SYSTEMD_INCLUDE_DIRS})
target_link_libraries(PardusOptimizer PRIVATE ${SYSTEMD_LIBRARIES})
qt_add_resources(HWControlCenter "qml_resources"
    PREFIX "/"
    FILES
        app/src/gui/main.qml
        app/src/gui/hwmon.qml
        app/src/gui/details.qml
        app/images/github.svg
        app/images/language.svg
        app/images/processor-icon.svg
        app/images/back.svg
        app/images/speedometer.svg
        app/images/shutdown.svg
        app/images/explorer.svg
        app/fonts/Inter_18pt-Black.ttf
        app/fonts/PlusJakartaSans-Bold.ttf
)
qt_add_translations(HWControlCenter TS_FILES app/src/hwcenter_tr.ts)
set_target_properties(HWControlCenter PROPERTIES OUTPUT_NAME "Optimization Control Center")
set_target_properties(PardusOptimizer PROPERTIES OUTPUT_NAME "pardus-optimizer")
target_link_libraries(HWControlCenter PRIVATE
    Qt6::Quick
    Qt6::Qml
    Qt6::QuickControls2
)
set(CPACK_PACKAGING_INSTALL_PREFIX "/usr")
install(TARGETS HWControlCenter PardusOptimizer
    RUNTIME DESTINATION lib/pardushw
)
install(FILES ${CMAKE_SOURCE_DIR}/daemon/pardus-optimizer.service
    DESTINATION lib/systemd/system)
install(FILES ${CMAKE_SOURCE_DIR}/daemon/pardus-optimizer.conf
    DESTINATION /etc)
install(FILES ${CMAKE_SOURCE_DIR}/app/images/processor-icon.svg
    DESTINATION share/icons/hicolor/scalable/apps
    RENAME pardushw.svg)
file(WRITE ${CMAKE_BINARY_DIR}/packaging/pardushw.desktop
[=[[Desktop Entry]
Type=Application
Name=Hardware Control Center
Name[tr]=Donanım Kontrol Merkezi
Comment=Monitor hardware and optimize system performance
Comment[tr]=Donanımı izle ve sistem performansını optimize et
Icon=pardushw
Exec="/usr/lib/pardushw/Optimization Control Center"
Categories=System;Utility;
Keywords=hardware;monitor;optimize;donanım;izleme;
]=])
install(FILES ${CMAKE_BINARY_DIR}/packaging/pardushw.desktop
    DESTINATION share/applications)
file(WRITE ${CMAKE_BINARY_DIR}/packaging/postinst
[=[#!/bin/sh
set -e
if [ "$1" = "configure" ]; then
    systemctl daemon-reload >/dev/null 2>&1 || true
    systemctl enable --now pardus-optimizer.service >/dev/null 2>&1 || true
fi
exit 0
]=])
file(WRITE ${CMAKE_BINARY_DIR}/packaging/prerm
[=[#!/bin/sh
systemctl disable --now pardus-optimizer.service >/dev/null 2>&1 || true
exit 0
]=])
file(WRITE ${CMAKE_BINARY_DIR}/packaging/postrm
[=[#!/bin/sh
systemctl daemon-reload >/dev/null 2>&1 || true
exit 0
]=])
file(CHMOD ${CMAKE_BINARY_DIR}/packaging/postinst ${CMAKE_BINARY_DIR}/packaging/prerm ${CMAKE_BINARY_DIR}/packaging/postrm
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_BINARY_DIR}/packaging/postinst;${CMAKE_BINARY_DIR}/packaging/prerm;${CMAKE_BINARY_DIR}/packaging/postrm")
set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_NAME "pardushw")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Ahmet <ahmetcikko@gmail.com>")
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "libqt6core6, libqt6gui6, libqt6qml6, libqt6quick6, libqt6quickcontrols2-6, libsystemd0, qml6-module-qtquick, qml6-module-qtquick-window, qml6-module-qtquick-controls, qml6-module-qtquick-templates, qml6-module-qtquick-layouts, xdg-utils, polkitd")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Pardus Hardware Control and Optimization Center")
include(CPack)
