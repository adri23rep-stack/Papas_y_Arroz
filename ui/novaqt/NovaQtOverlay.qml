import QtQuick
import QtQuick.Window

Item {
    id: overlay
    property bool pairingVisible: false
    property bool qualityVisible: false

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


    // Uses the same quality setting the paired iPhone reads via /settings/get.
    Item {
        anchors.fill: parent
        visible: overlay.qualityVisible
        z: 32
        Rectangle {
            anchors.fill: parent
            color: "#DE000000"
            MouseArea {
                anchors.fill: parent
                onClicked: overlay.qualityVisible = false
            }
        }
        Rectangle {
            id: qualityPanel
            anchors.centerIn: parent
            width: Math.min(520,overlay.width - 70)
            height: Math.min(600,overlay.height - 24)
            radius: 20
            color: "#191821"
            border.color: "#595067"
            border.width: 1
            Text {
                x: 26; y: 18
                text: "Calidad de video"
                color: "white"
                font.family: "Segoe UI"
                font.pixelSize: 24
                font.weight: Font.DemiBold
            }
            Text {
                x: parent.width - 40
                y: 12
                text: "×"
                color: "white"
                font.pixelSize: 27
                MouseArea {
                    anchors.fill: parent
                    onClicked: overlay.qualityVisible = false
                }
            }
            Item {
                anchors.fill: parent
                visible: !novaHome.veyraActive
            Repeater {
                model: ["Compatible (CPU)", "Equilibrado", "Nitidez alta (Lanczos)",
                        "Detalle extra (CAS)", "GPU FSRCNNX"]
                delegate: Rectangle {
                    required property int index
                    required property string modelData
                    x: 24; y: 67 + index * 43
                    width: qualityPanel.width - 48
                    height: 39
                    radius: 10
                    color: novaHome.videoPreset === index ? "#463366" :
                           qualityMouse.containsMouse ? "#302B3D" : "#24222D"
                    border.color: novaHome.videoPreset === index ? "#A78BFA" : "#403A4D"
                    Accessible.role: Accessible.RadioButton
                    Accessible.name: modelData
                    Accessible.onPressAction: novaHome.setVideoPreset(index)
                    Text {
                        x: 15
                        anchors.verticalCenter: parent.verticalCenter
                        text: (novaHome.videoPreset === index ? "◉  " : "○  ") + modelData
                        color: "white"
                        font.family: "Segoe UI"
                        font.pixelSize: 15
                    }
                    MouseArea {
                        id: qualityMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: novaHome.setVideoPreset(index)
                    }
                }
            }
            Text {
                x: 26; y: 287
                text: "MotionBoost · FPS"
                color: "white"
                font.family: "Segoe UI"
                font.pixelSize: 17
                font.weight: Font.DemiBold
            }
            Repeater {
                model: ["Desactivado", "Suavizado ligero (MPV)",
                        "RIFE GPU Vulkan · 2x (experimental)"]
                delegate: Rectangle {
                    required property int index
                    required property string modelData
                    x: 24; y: 316 + index * 40
                    width: qualityPanel.width - 48
                    height: 35
                    radius: 9
                    color: novaHome.motionMode === index ? "#463366" :
                           motionMouse.containsMouse ? "#302B3D" : "#24222D"
                    border.color: novaHome.motionMode === index ? "#A78BFA" : "#403A4D"
                    Accessible.role: Accessible.RadioButton
                    Accessible.name: modelData
                    Accessible.onPressAction: novaHome.setMotionMode(index)
                    Text {
                        x: 12
                        anchors.verticalCenter: parent.verticalCenter
                        text: (novaHome.motionMode === index ? "◉  " : "○  ") + modelData
                        color: "white"
                        font.family: "Segoe UI"
                        font.pixelSize: 13
                    }
                    MouseArea {
                        id: motionMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: novaHome.setMotionMode(index)
                    }
                }
            }
            Text {
                x: 26; y: 443
                width: parent.width - 52
                text: novaHome.gpuRifeReady
                    ? "GPU RIFE preparado. 24→48 / 30→60 FPS; monitorea la sincronización."
                    : "RIFE GPU no instalado. Extrae el ZIP GPU RIFE adicional; el video normal sigue disponible."
                color: "#E9BC89"
                font.family: "Segoe UI"
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
            Text {
                x: 26; y: 487
                width: parent.width - 52
                color: "#C4B4F8"
                font.pixelSize: 13
                font.family: "Segoe UI"
                text: novaHome.playbackNotice.length > 0 ? novaHome.playbackNotice :
                    "Se guardará para el próximo video."
                wrapMode: Text.WordWrap
            }
            Text {
                x: 26; y: 526
                width: parent.width - 52
                height: Math.max(45,qualityPanel.height - 533)
                color: "#B9B4C5"
                font.family: "Segoe UI"
                font.pixelSize: 12
                wrapMode: Text.WordWrap
                text: novaHome.playing ? novaHome.playbackDiagnostics :
                    "Al cambiar de modo, Nova reinicia solo MPV y recupera la posición. " +
                    "En TV en vivo se reconecta al canal."
            }
            }
            Column {
                x: 24; y: 70
                width: parent.width - 48
                spacing: 12
                visible: novaHome.veyraActive
                Text {
                    width: parent.width
                    text: "Veyra · " + (novaHome.veyraState.fgBackend || "motor conectado")
                    color: "#C4B4F8"; font.pixelSize: 18
                }
                Repeater {
                    model: [
                        {key:"veyra_fg", field:"fgEnabled", label:"Generación de cuadros"},
                        {key:"veyra_sr", field:"srEnabled", label:"Superresolución"},
                        {key:"veyra_nr", field:"nrEnabled", label:"Mejora NR"},
                        {key:"veyra_hdr", field:"hdrEnabled", label:"HDR"}
                    ]
                    delegate: Rectangle {
                        required property var modelData
                        width: qualityPanel.width - 48; height: 44; radius: 10
                        property bool selected: !!novaHome.veyraState[modelData.field]
                        color: selected ? "#463366" : "#24222D"
                        border.color: selected ? "#A78BFA" : "#403A4D"
                        Accessible.role: Accessible.CheckBox
                        Accessible.name: modelData.label
                        Accessible.checked: selected
                        Accessible.onPressAction: novaHome.setVeyraSetting(modelData.key, selected ? 0 : 1)
                        Text {
                            x: 14; anchors.verticalCenter: parent.verticalCenter
                            text: modelData.label + (parent.selected ? " · Activado" : " · Desactivado")
                            color: "white"; font.pixelSize: 15
                        }
                        MouseArea {
                            anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: novaHome.setVeyraSetting(parent.modelData.key, parent.selected ? 0 : 1)
                        }
                    }
                }
                Text { text: "Multiplicador de cuadros"; color: "white"; font.pixelSize: 15 }
                Flow {
                    width: parent.width; spacing: 8
                    Repeater {
                        model: novaHome.veyraState.fgMultiplierChoices || []
                        delegate: Rectangle {
                            required property var modelData
                            width: 57; height: 37; radius: 8
                            color: Number(modelData.id) === novaHome.veyraState.fgMultiplier ? "#463366" : "#24222D"
                            border.color: "#A78BFA"
                            Accessible.role: Accessible.RadioButton
                            Accessible.name: modelData.label
                            Accessible.onPressAction: novaHome.setVeyraSetting("veyra_multiplier", Number(modelData.id))
                            Text { anchors.centerIn: parent; text: modelData.label; color: "white" }
                            MouseArea {
                                anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                onClicked: novaHome.setVeyraSetting("veyra_multiplier", Number(parent.modelData.id))
                            }
                        }
                    }
                }
                Text {
                    width: parent.width; wrapMode: Text.WordWrap
                    text: novaHome.playbackDiagnostics
                    color: "#C4B4F8"; font.pixelSize: 14
                }
                Text {
                    width: parent.width; wrapMode: Text.WordWrap
                    text: novaHome.playbackNotice
                    color: "#E9BC89"; font.pixelSize: 13
                }
                Text {
                    width: parent.width; wrapMode: Text.WordWrap
                    text: "El estado lo confirma Veyra. Activar una mejora no garantiza más FPS; depende del video y la GPU."
                    color: "#B9B4C5"; font.pixelSize: 12
                }
            }
            // Shown ONLY when the user explicitly starts the stock Win32
            // pilot. Catalog URL validation remains unchanged; this one
            // fixed local test.mp4 is already allowed by catalog.cpp.
            Rectangle {
                visible: novaHome.win32PilotEnabled
                x: 24
                y: qualityPanel.height - 58
                width: qualityPanel.width - 48
                height: 44
                radius: 10
                color: win32SampleMouse.containsMouse ? "#514079" : "#40305E"
                border.color: "#AA90F5"
                Accessible.role: Accessible.Button
                Accessible.name: "Probar Veyra con test.mp4 local"
                Accessible.onPressAction: {
                    if (novaHome.playLocalPilotSample())
                        overlay.qualityVisible = false
                }
                Text {
                    anchors.centerIn: parent
                    text: "Probar Veyra Win32 · test.mp4"
                    color: "white"
                    font.pixelSize: 15
                    font.family: "Segoe UI"
                }
                MouseArea {
                    id: win32SampleMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (novaHome.playLocalPilotSample())
                            overlay.qualityVisible = false
                    }
                }
            }
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
