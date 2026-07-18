import QtQuick
import QtQuick.Window

Window {
    id: detailsWindow
    property string kind: ""
    property var sections: []
    visible: true
    width: Math.round(Screen.desktopAvailableWidth * 0.26)
    height: Math.round(Screen.desktopAvailableHeight * 0.66)
    minimumWidth: width
    maximumWidth: width
    minimumHeight: height
    maximumHeight: height
    title: kind
    color: "transparent"
    function refresh() { sections = backend.details(kind) }
    function shade(f, d, a) {
        var g = 0.38 - 0.17 * Math.min(f, 1.0) + d
        return Qt.rgba(g, g, g + 0.008, a)
    }
    Component.onCompleted: refresh()
    FontLoader {
        id: boldFont
        source: "qrc:/app/fonts/Inter_18pt-Black.ttf"
    }
    FontLoader {
        id: rowFont
        source: "qrc:/app/fonts/PlusJakartaSans-Bold.ttf"
    }
    Timer {
        interval: 1000
        running: detailsWindow.visible
        repeat: true
        onTriggered: detailsWindow.refresh()
    }
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#da5e5e5e" }
            GradientStop { position: 1.0; color: "#da343435" }
        }
    }
    Flickable {
        anchors.fill: parent
        contentHeight: column.height + Math.round(detailsWindow.height * 0.04)
        clip: true
        Column {
            id: column
            x: Math.round(detailsWindow.width * 0.04)
            y: Math.round(detailsWindow.height * 0.02)
            width: Math.round(detailsWindow.width * 0.92)
            spacing: Math.round(detailsWindow.height * 0.018)
            Repeater {
                model: detailsWindow.sections
                Column {
                    property real posf: detailsWindow.sections.length > 1 ? index / (detailsWindow.sections.length - 1) : 0
                    property real poss: 1 / Math.max(1, detailsWindow.sections.length)
                    width: column.width
                    Rectangle {
                        width: parent.width
                        height: Math.round(detailsWindow.height * 0.038)
                        radius: Math.round(height * 0.25)
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0.0; color: detailsWindow.shade(posf, 0.20, 0.85) }
                            GradientStop { position: 1.0; color: detailsWindow.shade(posf, 0.05, 0.45) }
                        }
                        Text {
                            text: modelData.title
                            color: "#ffffff"
                            font.family: boldFont.name
                            font.pixelSize: Math.round(parent.height * 0.5)
                            anchors.verticalCenter: parent.verticalCenter
                            x: Math.round(parent.width * 0.03)
                        }
                    }
                    Rectangle {
                        width: parent.width
                        height: rowsColumn.height
                        radius: Math.round(detailsWindow.height * 0.008)
                        gradient: Gradient {
                            GradientStop { position: 0.0; color: detailsWindow.shade(posf, 0.03, 0.55) }
                            GradientStop { position: 1.0; color: detailsWindow.shade(posf + poss, 0.03, 0.55) }
                        }
                        Column {
                            id: rowsColumn
                            width: parent.width
                            Repeater {
                                model: modelData.rows
                                Rectangle {
                                    width: rowsColumn.width
                                    height: Math.round(detailsWindow.height * 0.034)
                                    color: index % 2 ? "#1a000000" : "#0dffffff"
                                    Text {
                                        text: modelData.k
                                        color: "#d8d8d8"
                                        font.family: rowFont.name
                                        font.pixelSize: Math.round(parent.height * 0.42)
                                        anchors.verticalCenter: parent.verticalCenter
                                        x: Math.round(parent.width * 0.03)
                                        width: Math.round(parent.width * 0.42)
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        text: modelData.v
                                        color: "#ffffff"
                                        font.family: rowFont.name
                                        font.pixelSize: Math.round(parent.height * 0.42)
                                        anchors.verticalCenter: parent.verticalCenter
                                        x: Math.round(parent.width * 0.46)
                                        width: Math.round(parent.width * 0.51)
                                        elide: Text.ElideRight
                                        horizontalAlignment: Text.AlignRight
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    onClosing: (close) => { Qt.callLater(detailsWindow.destroy) }
}
