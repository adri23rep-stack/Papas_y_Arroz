import QtQuick
import QtQuick.Window
import QtQuick.Effects
import QtQuick.Shapes

Window {
    id: root
    width: 1280
    height: 800
    minimumWidth: 900
    minimumHeight: 620
    visible: true
    title: "Xuper TV · Nova Qt"
    color: "#000000"

    // Curated wide-format 2026 film/series stills. Never upscale tiny
    // channel icons as a full-screen wallpaper.
    // A backdrop and its title must be selected together. On a failed URL
    // the next image AND its own title are selected, never a mismatched title.
    property var backdropSources: [
        {
            title: "La Odisea",
            url: "https://assets.eyefilm.nl/images/production/_1920x1080_crop_center-center_none/1632957/still_The-Odyssey-Christopher-Nolan-US-GB-2026-1.webp"
        },
        {
            title: "Wildwood",
            url: "https://d32qys9a6wm9no.cloudfront.net/images/movies/backdrop/ZVofAle5TZmIEieUj7WVjT2NYNyBAux8_1920x1080.jpg?t=1778746400"
        },
        {
            title: "Proyecto Fin del Mundo",
            url: "https://www.mensjournal.com/.image/c_fill%2Cg_faces%3Acenter/NDI6MDAwMDAwMDAxMTE2OTA4/project-hail-mary-1-header.jpg"
        },
        {
            title: "VisionQuest",
            url: "https://cdn.observatoriodocinema.com.br/2025/10/vision-quest-1536x646.jpeg"
        }
    ]
    property int backdropStart: Math.floor(Math.random() * backdropSources.length)
    property int backdropAttempt: 0
    readonly property int featuredIndex: backdropAttempt < backdropSources.length
        ? (backdropStart + backdropAttempt) % backdropSources.length : -1
    readonly property string featuredTitle: featuredIndex >= 0
        ? backdropSources[featuredIndex].title : "DESTACADOS"
    property string toast: ""
    // Larger cinematic hero while keeping all three browse rails together.
    // Shorter windows can scroll down; fullscreen uses its extra real height.
    readonly property real homeRowsTop:
        Math.max(306, Math.min(height * 0.44, height - 595))
    readonly property real backdropBottom: homeRowsTop + 43

    function notifyPreview(subject) {
        toast = subject + " · por ahora solo estamos probando Inicio"
        toastTimer.restart()
    }

    Timer {
        id: toastTimer
        interval: 3200
        onTriggered: root.toast = ""
    }

    Flickable {
        id: page
        anchors.fill: parent
        contentWidth: width
        contentHeight: Math.max(height, rows.y + rows.height + 42)
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Item {
            id: hero
            width: page.width
            height: root.backdropBottom
            anchors.top: parent.top

            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    GradientStop { position: 0; color: "#20202c" }
                    GradientStop { position: 1; color: "#000000" }
                }
            }
            Image {
                id: wallpaper
                anchors.fill: parent
                fillMode: Image.PreserveAspectCrop
                sourceSize.width: 1920
                sourceSize.height: 1080
                asynchronous: true
                cache: true
                smooth: true
                mipmap: true
                source: root.featuredIndex >= 0
                    ? root.backdropSources[root.featuredIndex].url : ""
                onStatusChanged: {
                    if (status === Image.Error && root.backdropAttempt < root.backdropSources.length)
                        root.backdropAttempt += 1
                }
            }
            // Film cover fades to true black just above the TV rail.
            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    GradientStop { position: 0.0; color: "#10000000" }
                    GradientStop { position: 0.30; color: "#1A000000" }
                    GradientStop { position: 0.55; color: "#44000000" }
                    GradientStop { position: 0.78; color: "#AB000000" }
                    GradientStop { position: 0.94; color: "#F1000000" }
                    GradientStop { position: 1.0; color: "#FF000000" }
                }
            }
        }

        Text {
            id: featuredTitle
            objectName: "novaFeaturedTitle"
            x: 48
            y: Math.max(72, rows.y - 181)
            width: Math.max(270, page.width - 96)
            text: root.featuredTitle
            color: "#FFFFFF"
            font.family: "Segoe UI"
            font.pixelSize: page.width >= 1600 ? 52 : 43
            font.weight: Font.Bold
            font.letterSpacing: 0.3
            elide: Text.ElideRight
            maximumLineCount: 2
            wrapMode: Text.WordWrap
            renderType: Text.QtRendering
            style: Text.Raised
            styleColor: "#B8000000"
            opacity: wallpaper.status === Image.Ready ? 1 : 0.92
            Behavior on opacity {
                NumberAnimation { duration: 280; easing.type: Easing.OutCubic }
            }
        }

        Column {
            id: rows
            x: 0
            y: root.homeRowsTop
            width: page.width
            spacing: 22

            Repeater {
                model: 3
                delegate: Item {
                    id: section
                    required property int index
                    property int sectionId: index
                    property string sectionTitle: sectionId === 0 ? "TV en vivo" :
                                                  sectionId === 1 ? "Series" : "Películas"
                    property var sectionModel: sectionId === 0 ? novaHome.channels :
                                               sectionId === 1 ? novaHome.series : novaHome.movies
                    width: rows.width
                    height: 176

                    Item {
                        id: heading
                        x: 47
                        y: 0
                        width: 300
                        height: 32
                        transformOrigin: Item.Left
                        scale: headingMouse.containsMouse ? 1.035 : 1.0
                        Behavior on scale {
                            NumberAnimation { duration: 170; easing.type: Easing.OutCubic }
                        }

                        Shape {
                            id: vectorIcon
                            x: 1
                            y: 3
                            width: 27
                            height: 27
                            preferredRendererType: Shape.CurveRenderer
                            opacity: headingMouse.containsMouse ? 1 : 0.82
                            Behavior on opacity {
                                NumberAnimation { duration: 160 }
                            }
                            ShapePath {
                                strokeColor: "#F5F5F7"
                                strokeWidth: 1.9
                                fillColor: "transparent"
                                capStyle: ShapePath.RoundCap
                                joinStyle: ShapePath.RoundJoin
                                PathSvg {
                                    path: section.sectionId === 0
                                        ? "M3 5H21V18H3Z M8 22H16 M12 18V22"
                                        : section.sectionId === 1
                                        ? "M5 4H19V21H5Z M9 8H15 M9 12H15 M9 16H13"
                                        : "M3 9L21 5V9H3Z M3 9H21V21H3Z M9 10L12 13 M16 10L19 13"
                                }
                            }
                        }
                        Text {
                            x: 40
                            anchors.verticalCenter: parent.verticalCenter
                            text: section.sectionTitle
                            color: headingMouse.containsMouse ? "#E7D7FF" : "#FFFFFF"
                            Behavior on color {
                                ColorAnimation { duration: 170 }
                            }
                            font.family: "Segoe UI"
                            font.pixelSize: 22
                            font.weight: Font.DemiBold
                            renderType: Text.QtRendering
                        }
                        MouseArea {
                            id: headingMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.notifyPreview(section.sectionTitle)
                        }
                    }

                    ListView {
                        id: carousel
                        x: 46
                        y: 40
                        width: Math.max(0, section.width - 64)
                        height: 132
                        orientation: ListView.Horizontal
                        model: section.sectionModel
                        spacing: 8
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        interactive: true
                        reuseItems: true
                        maximumFlickVelocity: 1800
                        flickDeceleration: 4200

                        delegate: Item {
                            id: tile
                            required property var modelData
                            width: 196
                            height: 128
                            property bool hovered: tileMouse.containsMouse
                            z: hovered ? 10 : 0

                            // Animate the entire card inside a slightly larger
                            // delegate, so the raised edges are never clipped.
                            Item {
                                id: tileSurface
                                x: 3
                                y: hovered ? 4 : 8
                                width: 190
                                height: 112
                                transformOrigin: Item.Center
                                scale: tile.hovered ? 1.045 : 1.0
                                Behavior on y {
                                    NumberAnimation { duration: 170; easing.type: Easing.OutCubic }
                                }
                                Behavior on scale {
                                    NumberAnimation { duration: 170; easing.type: Easing.OutCubic }
                                }

                                Rectangle {
                                id: border
                                anchors.fill: parent
                                radius: 15
                                antialiasing: true
                                color: "#17171b"
                                border.color: tile.hovered ? "#CFB6FF" : "#383841"
                                border.width: tile.hovered ? 2 : 1
                                border.pixelAligned: false
                                Behavior on border.color {
                                    ColorAnimation { duration: 170 }
                                }
                            }

                            Image {
                                id: poster
                                anchors.fill: parent
                                anchors.margins: 1
                                source: tile.modelData.artwork || ""
                                fillMode: section.sectionId === 0
                                    ? Image.PreserveAspectFit : Image.PreserveAspectCrop
                                sourceSize.width: 520
                                asynchronous: true
                                cache: true
                                smooth: true
                                mipmap: true
                                visible: false
                            }
                            Rectangle {
                                id: tileMask
                                anchors.fill: parent
                                anchors.margins: 1
                                radius: 14
                                antialiasing: true
                                color: "#FFFFFF"
                                visible: false
                                layer.enabled: true
                                layer.smooth: true
                                layer.samples: 4
                            }
                            MultiEffect {
                                anchors.fill: parent
                                anchors.margins: 1
                                source: poster
                                maskEnabled: true
                                maskSource: tileMask
                                visible: poster.status === Image.Ready
                            }

                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.margins: 7
                                height: 32
                                radius: 8
                                antialiasing: true
                                color: tile.hovered ? "#D4000000" : "#B6000000"
                                Behavior on color {
                                    ColorAnimation { duration: 170 }
                                }
                            }
                            Text {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                anchors.leftMargin: 15
                                anchors.rightMargin: 14
                                anchors.bottomMargin: 12
                                color: "#FFFFFF"
                                text: tile.modelData.title || ""
                                font.family: "Segoe UI"
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                                maximumLineCount: 1
                                renderType: Text.QtRendering
                            }
                            MouseArea {
                                id: tileMouse
                                anchors.fill: parent
                                hoverEnabled: true
                                preventStealing: false
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.notifyPreview(tile.modelData.title)
                            }
                            } // tileSurface
                        }

                        WheelHandler {
                            target: null
                            onWheel: (event) => {
                                const delta = event.angleDelta.y !== 0
                                    ? event.angleDelta.y : event.angleDelta.x
                                carousel.contentX = Math.max(0,
                                    Math.min(Math.max(0, carousel.contentWidth - carousel.width),
                                        carousel.contentX - (delta / 120) * 218))
                                event.accepted = true
                            }
                        }
                    }

                    Text {
                        x: 49
                        y: 81
                        text: novaHome.loading ? "Cargando catálogo..." :
                              "No hay títulos disponibles"
                        visible: section.sectionModel.length === 0
                        color: "#85858e"
                        font.family: "Segoe UI"
                        font.pixelSize: 15
                    }
                }
            }
        }

        Text {
            x: 48
            y: rows.y + rows.height + 12
            text: novaHome.status
            color: "#787881"
            font.family: "Segoe UI"
            font.pixelSize: 12
            opacity: novaHome.loading || novaHome.channels.length === 0 ? 1 : 0.5
        }
    }

    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 24
        width: Math.min(root.width - 48, message.implicitWidth + 38)
        height: 42
        radius: 14
        color: "#E91A1A22"
        border.color: "#53515d"
        visible: root.toast !== ""
        z: 10
        Text {
            id: message
            anchors.centerIn: parent
            text: root.toast
            color: "#FFFFFF"
            font.family: "Segoe UI"
            font.pixelSize: 14
            elide: Text.ElideRight
        }
    }
}
