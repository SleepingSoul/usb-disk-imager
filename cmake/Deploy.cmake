# ---------------------------------------------------------------------------
# Installation and platform deployment.
# ---------------------------------------------------------------------------

include(GNUInstallDirs)

if (APPLE)
    install(TARGETS ${PROJECT_NAME} BUNDLE DESTINATION .)
elseif (WIN32)
    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION .)
    # A copy next to the executable, so a deployed build can be retuned without a rebuild.
    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/configs DESTINATION .)
else()
    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    install(FILES packaging/linux/usb-disk-imager.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
    install(FILES content/assets/icons/logo.svg
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps
        RENAME usb-disk-imager.svg)
    install(FILES packaging/linux/dev.tkatolikian.usbdiskimager.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo)
    # Lets members of the "disk" group image removable media without root.
    install(FILES packaging/linux/99-usb-disk-imager.rules
        DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/udev/rules.d)
endif()

# windeployqt / macdeployqt, and on Linux a copy of the Qt runtime beside the binary. That is what a
# self-contained tarball wants and exactly what a distro or snap package does not: there Qt arrives from
# the platform, and a second copy would both bloat the package and shadow it.
option(UDI_DEPLOY_QT_RUNTIME "Install a private copy of the Qt runtime next to the executable" ON)

if (UDI_DEPLOY_QT_RUNTIME)
    qt_generate_deploy_qml_app_script(
        TARGET ${PROJECT_NAME}
        OUTPUT_SCRIPT UDI_DEPLOY_SCRIPT
        MACOS_BUNDLE_POST_BUILD
        NO_UNSUPPORTED_PLATFORM_ERROR
    )
    install(SCRIPT ${UDI_DEPLOY_SCRIPT})
endif()

set(CPACK_PACKAGE_NAME "${UDI_APP_ID}")
set(CPACK_PACKAGE_VENDOR "${UDI_AUTHOR}")
set(CPACK_PACKAGE_CONTACT "${UDI_AUTHOR} <${UDI_AUTHOR_EMAIL}>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_DEBIAN_PACKAGE_SECTION "utils")

if (WIN32)
    set(CPACK_GENERATOR "ZIP")
elseif (APPLE)
    set(CPACK_GENERATOR "DragNDrop")
else()
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)
