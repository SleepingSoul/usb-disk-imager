qt_add_library(content STATIC)

set_source_files_properties(Style.qml Icons.qml
    PROPERTIES
        QT_QML_SINGLETON_TYPE TRUE
)

# RESOURCE_PREFIX "/" keeps the QML at :/content/..., which is where App.qml's own relative asset URLs
# and UiManager's entry-point URL expect it.
qt_add_qml_module(content
    URI "content"
    VERSION 1.0
    RESOURCE_PREFIX "/"
    QML_FILES
        App.qml
        Style.qml
        Icons.qml
        AppIcon.qml
        AppButton.qml
        AppCheckBox.qml
        Badge.qml
        Card.qml
        ConfirmWriteDialog.qml
        DeviceSelector.qml
        DigestRow.qml
        ElevationAlert.qml
        FilePathField.qml
        Header.qml
        IconButton.qml
        InlineAlert.qml
        LanguageSwitch.qml
        NavItem.qml
        OptionCard.qml
        ProgressDialog.qml
        ResultPanel.qml
        StatTile.qml
        AboutPage.qml
        ReadPage.qml
        VerifyPage.qml
        WritePage.qml
    RESOURCES
        assets/icons/logo.svg
)
