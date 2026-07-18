import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts

// Bu dosya uygulama için derlenen C++ dosyasıdır. Son uygulama bunu kullanacaktır.

Window {
    id: window
    visible: true
    property bool quitting: false
    property var hwmonWin: null
    width: Math.round(Screen.desktopAvailableWidth * 0.24)
    height: Math.round(Screen.desktopAvailableHeight * 0.6)
    title: qsTr("Hardware Control Center")
    minimumWidth: width
    maximumWidth: width
    minimumHeight: height
    maximumHeight: height
    color: "#27262c"
    onClosing: (close) => { window.quitting = true }
    FontLoader {
        id: boldFont
        source: "qrc:/app/fonts/Inter_18pt-Black.ttf"
    }
    FontLoader {
        id: buttonFont
        source: "qrc:/app/fonts/PlusJakartaSans-Bold.ttf"
    }
    Rectangle {
        id: topBar
        height: Math.round(parent.height * 0.12)
        width: Math.round(parent.width * 0.9)
        x: Math.round(parent.width * 0.05)
        y: Math.round(parent.height * 0.025)
        color: "#8c8c8c"
        radius: Math.round(height * 0.3)
        Text {
            text: qsTr("HARDWARE MONITORING\nCENTER")
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            font.family: boldFont.name
            font.pixelSize: Math.round(parent.height * 0.25)
            anchors.verticalCenter: parent.verticalCenter
            color: "#ffffff"
        }
        Image {
            source: "qrc:/app/images/processor-icon.svg"
            height: Math.round(parent.height * 0.6)
            width: Math.round(parent.height * 0.65)
            x: Math.round(parent.width * 0.04)
            anchors.verticalCenter: parent.verticalCenter
        }
    }
    Button {
        id: hardwareCheck
        x: Math.round(parent.width * 0.05)
        y: Math.round(parent.height * 0.55)
        height: Math.round(parent.height * 0.09)
        width: Math.round(parent.width * 0.9)
        Text {
            text: qsTr("Hardware Usages")
            color: "#282525"
            font.family: buttonFont.name
            font.pixelSize: Math.round(parent.height * 0.45)
            anchors.centerIn: parent
        }
        background: Rectangle {
            color: hardwareCheck.down ? Qt.darker("#d1cccc", 1.5) : "#d1cccc"
            radius: Math.round(parent.height * 0.12)
        }
        onClicked: {
            var comp = Qt.createComponent("hwmon.qml")
            hwmonWin = comp.createObject(null)
            hwmonWin.showHome.connect(function() { window.visible = true; hwmonWin = null })
            hwmonWin.visible = true
            window.visible = false
        }
    }
    Button {
        id: optimize
        x: hardwareCheck.x
        y: hardwareCheck.y + hardwareCheck.height + Math.round(parent.height * 0.02)
        height: hardwareCheck.height
        width: hardwareCheck.width
        Text {
            text: qsTr("Optimize System")
            color: "#282525"
            font.family: buttonFont.name
            font.pixelSize: Math.round(parent.height * 0.45)
            anchors.centerIn: parent
        }
        background: Rectangle {
            color: optimize.down ? Qt.darker("#d1cccc", 1.5) : "#d1cccc"
            radius: Math.round(parent.height * 0.12)
        }
        onClicked: {
            optimizePopup.running = true
            optimizePopup.open()
            backend.startOptimize()
        }
    }
    Button {
        id: github
        x: hardwareCheck.x
        y: optimize.y + optimize.height + Math.round(parent.height * 0.02)
        height: hardwareCheck.height
        width: hardwareCheck.width
        Text {
            text: qsTr("Support Our GitHub")
            color: "#ffffff"
            font.family: buttonFont.name
            font.pixelSize: Math.round(parent.height * 0.45)
            anchors.centerIn: parent
            width: Math.round(github.width * 0.62)
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            fontSizeMode: Text.Fit
        }
        Image {
            source: "qrc:/app/images/github.svg"
            height: Math.round(parent.height * 0.7)
            width: height
            x: Math.round(parent.width * 0.06)
            anchors.verticalCenter: parent.verticalCenter
        }
        background: Rectangle {
            color: github.down ? Qt.darker("#494141", 1.5) : "#494141"
            radius: Math.round(parent.height * 0.12)
        }
        onClicked: Qt.openUrlExternally("https://github.com/ahmetcikko")
    }
    Button {
        id: exit
        x: Math.round(parent.width * 0.51)
        y: github.y + github.height + Math.round(parent.height * 0.02)
        height: hardwareCheck.height
        width: height
        Text {
            text: qsTr("EXIT")
            color: "#ffffff"
            font.family: boldFont.name
            font.pixelSize: Math.round(exit.height * 0.3)
            anchors.centerIn: parent
        }
        background: Rectangle {
            color: exit.down ? Qt.darker("#e00000", 1.5) : "#e00000"
            radius: Math.round(parent.height * 0.12)
        }
        onClicked: {
            window.quitting = true
            if (hwmonWin) hwmonWin.quitting = true
            Qt.quit()
        }
    }
    Button {
        id: language
        width: exit.width
        height: width
        x: Math.round(parent.width * 0.49 - width)
        y: exit.y
        background: Rectangle {
            color: language.down ? Qt.darker("#545454", 1.5) : "#545454"
            radius: Math.round(parent.height * 0.12)
        }
        Image {
            source: "qrc:/app/images/language.svg"
            anchors.centerIn: parent
            width: Math.round(parent.width * 0.7)
            height: width
        }
        onClicked: languagePopup.open()
    }

/* -------------------------------------------------------------------------------------------------- */

    Popup {
        id: languagePopup
        anchors.centerIn: parent
        width: Math.round(parent.width * 0.5)
        height: Math.round(parent.height * 0.3)
        modal: true
        background: Rectangle {
            color: "#4e4e4e"
            radius: Math.round(languagePopup.height * 0.04)
            border.color: "#0e0d0d"
            border.width: 1
        }
        Column {
            anchors.centerIn: parent
            width: Math.round(languagePopup.width * 0.75)
            height: Math.round(languagePopup.height * 0.6)
            spacing: Math.round(languagePopup.height * 0.06)
            Button {
                id: turkishOption
                width:parent.width
                height: Math.round(languagePopup.height * 0.27)
                Text {    
                    text: "Türkçe"
                    font.pixelSize: Math.round(turkishOption.width * 0.15)
                    font.family: boldFont.name
                    anchors.verticalCenter: parent.verticalCenter
                }
                background: Rectangle {
                    color: turkishOption.down ? "#3a3636" : "transparent"
                    radius: Math.round(turkishOption.height * 0.12)
                }
                onClicked: {
                    backend.setLanguage("tr")
                    languagePopup.close()
                }
            }
            Button {
                id: englishOption
                width: parent.width
                height: Math.round(languagePopup.height * 0.27)
                Text {    
                    text: "English"
                    font.pixelSize: Math.round(englishOption.width * 0.15)
                    font.family: boldFont.name
                    anchors.verticalCenter: parent.verticalCenter
                }
                background: Rectangle {
                    color: englishOption.down ? "#3a3636" : "transparent"
                    radius: Math.round(englishOption.height * 0.12)
                }
                onClicked: {
                    backend.setLanguage("en")
                    languagePopup.close()
                }
            }
        }
    }
    Popup {
        id: optimizePopup
        anchors.centerIn: parent
        width: Math.round(parent.width * 0.7)
        height: Math.round(parent.height * 0.24)
        modal: true
        padding: 0
        property bool running: false
        property bool ok: false
        property int freed: 0
        onOpened: {
            optimizeBar.width = Math.round(optimizeTrack.width * 0.3)
            optimizeBar.x = 0
        }
        background: Rectangle {
            color: "#4e4e4e"
            radius: Math.round(optimizePopup.height * 0.04)
            border.color: "#0e0d0d"
            border.width: 1
        }
        Text {
            text: optimizePopup.running ? qsTr("Optimizing…") : optimizePopup.ok ? qsTr("Freed %1 MB").arg(optimizePopup.freed) : qsTr("Optimization failed — service not running")
            color: "#ffffff"
            font.family: buttonFont.name
            font.pixelSize: Math.round(optimizePopup.height * 0.14)
            width: Math.round(optimizePopup.width * 0.86)
            wrapMode: Text.Wrap
            horizontalAlignment: Text.AlignHCenter
            anchors.horizontalCenter: parent.horizontalCenter
            y: Math.round(optimizePopup.height * 0.16)
        }
        Rectangle {
            id: optimizeTrack
            width: Math.round(optimizePopup.width * 0.8)
            height: Math.round(optimizePopup.height * 0.12)
            radius: height / 2
            color: "#2e2d33"
            anchors.horizontalCenter: parent.horizontalCenter
            y: Math.round(optimizePopup.height * 0.58)
            Rectangle {
                id: optimizeBar
                width: Math.round(optimizeTrack.width * 0.3)
                height: parent.height
                radius: height / 2
                color: optimizePopup.running ? "#8c8c8c" : optimizePopup.ok ? "#57d98c" : "#e05555"
                SequentialAnimation on x {
                    running: optimizePopup.running
                    loops: Animation.Infinite
                    NumberAnimation { to: optimizeTrack.width - optimizeBar.width; duration: 650; easing.type: Easing.InOutQuad }
                    NumberAnimation { to: 0; duration: 650; easing.type: Easing.InOutQuad }
                }
            }
        }
        Connections {
            target: backend
            function onOptimizeDone(ok, freedMb) {
                optimizePopup.running = false
                optimizePopup.ok = ok
                optimizePopup.freed = freedMb
                if (ok) {
                    optimizeBar.x = 0
                    optimizeBar.width = optimizeTrack.width
                }
                closeTimer.start()
            }
        }
        Timer {
            id: closeTimer
            interval: 2500
            onTriggered: optimizePopup.close()
        }
    }
}
