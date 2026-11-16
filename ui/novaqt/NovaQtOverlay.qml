import QtQuick
import QtQuick.Window

Item {
    id: overlay
    property bool pairingVisible: false

    property double now: 0
    property bool gridFocused: false
    readonly property int columns: Math.max(1, Math.floor(entries.width / entries.cellWidth))
    readonly property bool atFirstRow: novaHome.selectedItem < columns
    function moveGrid(dx, dy) {
        novaHome.selectGridItem(novaHome.selectedItem + dx + dy * columns)
    }

    // All sections, episodes and folders stay in the new Qt interface.
    Rectangle {
        id: catalog
        anchors.fill: parent
        visible: novaHome.browsing
        color: "#0B0B10"
        z: 10
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            y: 68
            height: 64
            color: "#0B0B0E"
            Rectangle {
                x: 30
                y: 10
                width: 72
                height: 36
                radius: 11
                color: backMouse.containsMouse ? "#484057" : "#302B3B"
                Behavior on color { ColorAnimation { duration: 150 } }
                Text {
                    anchors.centerIn: parent
                    text: "Atrás"
                    font.family: "Segoe UI"
                    font.pixelSize: 15
                    color: "white"
                }
                MouseArea {
                    id: backMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: novaHome.back()
                }
            }
            Text {
                x: 124
                y: 9
                width: Math.max(160, overlay.width - 500)
                text: novaHome.pageTitle
                color: "white"
                font.family: "Segoe UI"
                font.pixelSize: 27
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
        }
        GridView {
            id: entries
            x: 34
            y: 138
            width: Math.max(200, parent.width - 66)
            height: Math.max(220, parent.height - 181)
            model: novaHome.items
            currentIndex: novaHome.selectedItem
            cellWidth: 248
            cellHeight: novaHome.pageTitle === "TV en vivo" ? 214 : 324
            clip: true
            cacheBuffer: 450
            boundsBehavior: Flickable.StopAtBounds

            delegate: NovaCard {
                required property var modelData
                required property int index
                entry: modelData
                channel: modelData.kind === 2 || novaHome.pageTitle === "TV en vivo"
                live: modelData.kind === 2 || (channel && modelData.kind === 0)
                now: overlay.now
                focused: overlay.gridFocused && entries.currentIndex === index
                onActivated: novaHome.openItem(index)
            }
            onCurrentIndexChanged: {
                if(currentIndex >= 0 && count > 0)
                    positionViewAtIndex(currentIndex,GridView.Contain)
            }
        }
        Text {
            anchors.centerIn: entries
            color: "#B5B0C2"
            visible: novaHome.items.length === 0
            text: novaHome.loading ? "Cargando..." : "No hay contenido en esta sección."
            font.pixelSize: 17
            font.family: "Segoe UI"
        }
        Text {
            x: 34
            y: parent.height - 29
            width: parent.width - 68
            text: novaHome.status
            elide: Text.ElideRight
            color: "#9691A3"
            font.family: "Segoe UI"
            font.pixelSize: 12
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 20
        anchors.horizontalCenter: parent.horizontalCenter
        width: Math.min(overlay.width - 44, playbackLabel.implicitWidth + 40)
        height: 39
        radius: 12
        color: "#D2262437"
        visible: novaHome.playing
        z: 20
        Text {
            id: playbackLabel
            anchors.centerIn: parent
            text: "▶ " + novaHome.playbackTitle + " · Atrás para detener"
            color: "#FFFFFF"
            font.family: "Segoe UI"
            font.pixelSize: 14
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: novaHome.stopPlayback()
        }
    }

    Item {
        anchors.fill: parent
        visible: overlay.pairingVisible
        z: 31
        Rectangle {
            anchors.fill: parent
            color: "#DF000000"
            MouseArea {
                anchors.fill: parent
                onClicked: overlay.pairingVisible = false
            }
        }
        Rectangle {
            id: pairing
            anchors.centerIn: parent
            width: 388
            height: 462
            radius: 21
            antialiasing: true
            color: "#171723"
            border.color: "#5E586F"
            Text {
                y: 21
                anchors.horizontalCenter: parent.horizontalCenter
                text: "Vincular control del iPhone"
                color: "white"
                font.family: "Segoe UI"
                font.pixelSize: 21
                font.weight: Font.DemiBold
            }
            Rectangle {
                x: 74
                y: 68
                width: 240
                height: 240
                radius: 10
                color: "white"
                Image {
                    anchors.fill: parent
                    anchors.margins: 10
                    source: novaHome.pairingQr
                    fillMode: Image.PreserveAspectFit
                    smooth: false
                    visible: novaHome.remoteReady && novaHome.pairingQr.length > 0
                }
                Text {
                    anchors.centerIn: parent
                    text: "QR no disponible"
                    color: "#35313E"
                    visible: !novaHome.remoteReady || novaHome.pairingQr.length === 0
                    font.pixelSize: 16
                }
            }
            Text {
                y: 322
                anchors.horizontalCenter: parent.horizontalCenter
                text: "IP: " + novaHome.pairingAddress + ":47831"
                color: "#D9D4E7"
                font.family: "Segoe UI"
                font.pixelSize: 15
            }
            Text {
                y: 352
                anchors.horizontalCenter: parent.horizontalCenter
                text: "PIN: " + (novaHome.remoteReady ? novaHome.pairingPin : "No disponible")
                color: "#FFFFFF"
                font.family: "Segoe UI"
                font.pixelSize: 24
                font.weight: Font.DemiBold
            }
            Text {
                y: 396
                width: parent.width - 55
                anchors.horizontalCenter: parent.horizontalCenter
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: novaHome.remoteConnected ? "iPhone conectado" :
                    novaHome.remoteReady ? "Usa la misma red Wi-Fi en ambos equipos." :
                                           "Control no disponible. Revisa el puerto 47831."
                color: novaHome.remoteConnected ? "#A78BFA" : "#ADA8BF"
                font.family: "Segoe UI"
                font.pixelSize: 14
            }
            Text {
                x: parent.width - 34
                y: 9
                text: "×"
                color: "#EAE5F1"
                font.pixelSize: 26
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: overlay.pairingVisible = false
                }
            }
        }
    }

}
