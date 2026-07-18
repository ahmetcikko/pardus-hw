import QtQuick
import QtQuick.Window
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: hardwareWindow
    property bool quitting: false
    signal showHome()
    function usage_for(t) {
        if (t === "CPU") return backend.cpuText
        if (t === "RAM") return backend.ramText
        if (t === "Disk") return backend.diskText
        if (t === "GPU") return backend.gpuText
        return backend.netText
    }
    function label_for(t) {
        if (t === "CPU") return qsTr("CPU")
        if (t === "RAM") return qsTr("RAM")
        if (t === "Disk") return qsTr("Disk")
        if (t === "GPU") return qsTr("GPU")
        return qsTr("Network")
    }
    visible: false
    minimumWidth: Math.round(Screen.desktopAvailableWidth * 0.5)
    minimumHeight: Math.round(Screen.desktopAvailableHeight * 0.6)
    color: "transparent"
    FontLoader {
        id: boldFont
        source: "qrc:/app/fonts/Inter_18pt-Black.ttf"
    }
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#da5e5e5e" }
            GradientStop { position: 1.0; color: "#da343435" }
        }
    }
    Button {
        id: exitHW
        width: Math.round(hardwareWindow.minimumWidth * 0.06)
        height: width
        x: Math.round(parent.width * 0.002)
        y: x
        background: Rectangle {
            color: exitHW.down ? Qt.darker("#757575", 1.5) : "#757575"
            radius: Math.round(parent.height * 0.2)
        }
        Image {
            source: "qrc:/app/images/back.svg"
            anchors.centerIn: parent
            width: Math.round(parent.width * 0.8)
            height: width
        }
        onClicked: {
            showHome()
            Qt.callLater(hardwareWindow.destroy)
        }
    }
    RowLayout {
        width: Math.round(parent.width * 0.8)
        height: Math.round(parent.height * 0.2)
        anchors.horizontalCenter: parent.horizontalCenter
        y: Math.round(parent.height * 0.05)
        spacing: Math.round(parent.width * 0.02)
        Repeater {
            model: ListModel {
                ListElement { type: "CPU" }
                ListElement { type: "RAM" }
                ListElement { type: "Disk" }
                ListElement { type: "GPU" }
                ListElement { type: "Network" }
            }
            delegate: Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: buttonArea.pressed ? '#33242121' : "transparent"
                radius: Math.round(width * 0.15)
                Text {
                    id: hwtype
                    text: hardwareWindow.label_for(type)
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: Math.round(parent.height * 0.18)
                    font.family: boldFont.name
                    font.pixelSize: Math.round(parent.height * 0.16)
                }
                Text {
                    text: hardwareWindow.usage_for(type)
                    font.pixelSize: Math.round(parent.height * 0.12)
                    anchors.horizontalCenter: parent.horizontalCenter
                    horizontalAlignment: Text.AlignHCenter
                    y: Math.round(hwtype.y + hwtype.height + parent.height * 0.05)
                }
                MouseArea {
                    id: buttonArea
                    anchors.fill: parent
                    onClicked: {
                        var comp = Qt.createComponent("details.qml")
                        comp.createObject(hardwareWindow, { kind: type })
                    }
                }
            }
        }
    }
    ColumnLayout {
        width: Math.round(parent.width * 0.7)
        height: Math.round(parent.height * 0.6)
        anchors.horizontalCenter: parent.horizontalCenter
        y: Math.round(parent.height * 0.3)
        spacing: Math.round(parent.height * 0.005)
        Repeater {
            model: backend.heavyTasks
            delegate: RowLayout {
                Layout.fillHeight: true
                Layout.fillWidth: true
                Text {
                    text: qsTr("%1 is using %2 MB (%3%)").arg(modelData.name).arg(modelData.mem).arg(modelData.pct)
                    color: modelData.pct >= 15 ? "#ff9d9d" : "#e8e8e8"
                    font.pixelSize: Math.round(hardwareWindow.minimumHeight * 0.04)
                    Layout.fillWidth: true
                }
                Rectangle {
                    property bool pressed: false
                    Layout.preferredWidth: Math.round(hardwareWindow.minimumHeight * 0.1)
                    Layout.preferredHeight: Layout.preferredWidth
                    color: pressed ? Qt.darker("#3a1b1919", 1.5) : "transparent"
                    radius: Math.round(height * 0.16)
                    Image {
                        source: "qrc:/app/images/shutdown.svg"
                        anchors.centerIn: parent
                        width: Math.round(parent.width * 0.7)
                        height: width
                    }
                    MouseArea {
                        anchors.fill: parent
                        onPressed: parent.pressed = true
                        onReleased: parent.pressed = false
                        onClicked: backend.killProcess(modelData.pid)
                    }
                }
                Rectangle {
                    property bool pressed: false
                    Layout.preferredWidth: Math.round(hardwareWindow.minimumHeight * 0.1)
                    Layout.preferredHeight: Layout.preferredWidth
                    color: pressed ? Qt.darker("#3a1b1919", 1.5) : "transparent"
                    radius: Math.round(height * 0.16)
                    Image {
                        source: "qrc:/app/images/speedometer.svg"
                        anchors.centerIn: parent
                        width: Math.round(parent.width * 0.7)
                        height: width
                    }
                    MouseArea {
                        anchors.fill: parent
                        onPressed: parent.pressed = true
                        onReleased: parent.pressed = false
                        onClicked: backend.throttleProcess(modelData.pid)
                    }
                }
                Rectangle {
                    property bool pressed: false
                    Layout.preferredWidth: Math.round(hardwareWindow.minimumHeight * 0.1)
                    Layout.preferredHeight: Layout.preferredWidth
                    color: pressed ? Qt.darker("#3a1b1919", 1.5) : "transparent"
                    radius: Math.round(height * 0.16)
                    Image {
                        source: "qrc:/app/images/explorer.svg"
                        anchors.centerIn: parent
                        width: Math.round(parent.width * 0.7)
                        height: width
                    }
                    MouseArea {
                        anchors.fill: parent
                        onPressed: parent.pressed = true
                        onReleased: parent.pressed = false
                        onClicked: backend.revealProcess(modelData.pid)
                    }
                }
            }
        }
    }
    onClosing: (close) => {
        quitting = true
        Qt.quit()
    }
}
