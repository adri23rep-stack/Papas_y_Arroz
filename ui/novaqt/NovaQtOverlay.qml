import QtQuick
import QtQuick.Window
import QtQuick.Effects

Item {
    id: overlay
    property bool pairingVisible: false

    // Small unobtrusive access to the existing iPhone pairing protocol.
    Rectangle {
        id: qrButton
        x: overlay.width - width - 25
        y: 20
        width: 98
        height: 36
        radius: 12
        z: 12
        antialiasing: true
        border.color: qrMouse.containsMouse ? "#CAB5F2" : "#777182"
        color: qrMouse.containsMouse ? "#D840364E" : "#BB171721"
        scale: qrMouse.containsMouse ? 1.04 : 1
        Behavior on scale {
            NumberAnimation { duration: 160; easing.type: Easing.OutCubic }
        }
        Text {
            anchors.centerIn: parent
            text: "QR / PIN"
            color: "white"
            font.family: "Segoe UI"
            font.pixelSize: 14
            font.weight: Font.DemiBold
        }
        MouseArea {
            id: qrMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: overlay.pairingVisible = !overlay.pairingVisible
        }
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
            height: 93
            color: "#1A1923"
            Rectangle {
                x: 30
                y: 25
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
                y: 21
                width: Math.max(160, overlay.width - 500)
                text: novaHome.pageTitle
                color: "white"
                font.family: "Segoe UI"
                font.pixelSize: 27
                font.weight: Font.DemiBold
                elide: Text.ElideRight
            }
            Rectangle {
                x: overlay.width - width - 40
                y: 25
                width: Math.min(285, Math.max(175, overlay.width * 0.24))
                height: 37
                color: "#33313F"
                radius: 10
                border.color: searchInput.activeFocus ? "#BAA1ED" : "#5D586D"
                Text {
                    x: 13
                    anchors.verticalCenter: parent.verticalCenter
                    text: "Buscar títulos..."
                    color: "#C0B9CC"
                    font.family: "Segoe UI"
                    font.pixelSize: 14
                    visible: searchInput.text.length === 0
                }
                TextInput {
                    id: searchInput
                    anchors.fill: parent
                    anchors.leftMargin: 13
                    anchors.rightMargin: 13
                    verticalAlignment: TextInput.AlignVCenter
                    color: "white"
                    font.family: "Segoe UI"
                    font.pixelSize: 14
                    clip: true
                    selectByMouse: true
                    onAccepted: novaHome.search(text)
                }
            }
        }
        GridView {
            id: entries
            x: 34
            y: 112
            width: Math.max(200, parent.width - 66)
            height: Math.max(220, parent.height - 161)
            model: novaHome.items
            currentIndex: novaHome.selectedItem
            cellWidth: 224
            cellHeight: novaHome.pageTitle === "TV en vivo" ? 205 : 314
            clip: true
            cacheBuffer: 450
            boundsBehavior: Flickable.StopAtBounds

            delegate: NovaCard {
                required property var modelData
                required property int index
                entry: modelData
                channel: modelData.kind === 2
                focused: entries.currentIndex === index
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
                color: novaHome.remoteConnected ? "#89E9B8" : "#ADA8BF"
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

    Shortcut {
        sequence: "Escape"
        onActivated: {
            if(overlay.pairingVisible) overlay.pairingVisible = false
            else if(novaHome.featuredInfoVisible) novaHome.closeFeaturedInfo()
            else novaHome.back()
        }
    }
}
