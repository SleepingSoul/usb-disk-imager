pragma Singleton
import QtQuick

// Design-time stand-in for UDI::LocalizationViewModel. selectLanguage only moves the highlight in
// LanguageSwitch.qml: the catalogues are installed by C++, so the interface itself stays English here.
QtObject {
    property string currentLanguage: "en"

    readonly property var availableLanguages: [
        { "code": "en", "name": "English" },
        { "code": "uk", "name": "Українська" }
    ]

    function selectLanguage(languageCode) {
        currentLanguage = languageCode;
    }
}
