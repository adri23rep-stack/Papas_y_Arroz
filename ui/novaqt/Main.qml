import QtQuick
import QtQuick.Window

Window {
    id: root
    width: 1280
    height: 800
    minimumWidth: 900
    minimumHeight: 620
    visible: true
    title: "Xuper TV · Nova Qt"
    color: "#0B0B0E"

    // The banner and rails grow together on bigger displays. There is never
    // a wall of cards pasted over the image in maximized/fullscreen mode.
    readonly property real homeRowsTop:
        Math.max(460,Math.min(580,height * 0.52))
    readonly property real backdropBottom: homeRowsTop + 85

    Flickable {
        id: page
        anchors.fill: parent
        contentWidth: width
        contentHeight: Math.max(height, rows.y + rows.height + 52)
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        NovaHero {
            id: hero
            width: page.width
            railTop: root.homeRowsTop
        }
        Column {
            id: rows
            x: 0
            y: root.homeRowsTop
            width: page.width
            spacing: 24

            NovaRail {
                width: rows.width
                heading: "Continuar viendo"
                entries: novaHome.continueWatching
                sectionId: -1
                channel: false
            }
            NovaRail {
                width: rows.width
                heading: "TV en vivo"
                entries: novaHome.channels
                sectionId: 0
                channel: true
            }
            NovaRail {
                width: rows.width
                heading: "Series"
                entries: novaHome.series
                sectionId: 1
                channel: false
            }
            NovaRail {
                width: rows.width
                heading: "Películas"
                entries: novaHome.movies
                sectionId: 2
                channel: false
            }
            Text {
                x: 54
                width: Math.max(300,page.width - 108)
                text: novaHome.status
                elide: Text.ElideRight
                color: "#85838F"
                font.family: "Segoe UI"
                font.pixelSize: 12
                leftPadding: 54
            }
        }
    }

    NovaQtOverlay {
        anchors.fill: parent
        z: 33
    }
    NovaTopNav {
        anchors.top: parent.top
        width: parent.width
        visible: !novaHome.browsing
        z: 36
    }

    // A real information view: never opens a random movie or reuses another
    // title's plot. Both actions reference the SAME featured catalog entry.
    Item {
        anchors.fill: parent
        visible: novaHome.featuredInfoVisible
        z: 50
        Rectangle {
            anchors.fill: parent
            color: "#DD000000"
            MouseArea {
                anchors.fill: parent
                onClicked: novaHome.closeFeaturedInfo()
            }
        }
        Rectangle {
            id: detailsCard
            width: Math.min(620,root.width - 80)
            height: Math.min(400,root.height - 90)
            anchors.centerIn: parent
            color: "#18171F"
            border.color: "#4B4558"
            border.width: 1
            radius: 19
            antialiasing: true
            Text {
                x: 30; y: 31
                width: parent.width - 65
                text: novaHome.featuredTitle
                font.family: "Segoe UI"
                font.pixelSize: 31
                font.weight: Font.Bold
                color: "#FFFFFF"
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }
            Text {
                x: 31; y: 112
                width: parent.width - 65
                height: parent.height - 205
                text: novaHome.featuredDescription || "El catálogo no proporciona una sinopsis para este título."
                font.family: "Segoe UI"
                font.pixelSize: 17
                lineHeight: 1.25
                color: "#CBC9D1"
                wrapMode: Text.WordWrap
                maximumLineCount: 7
                elide: Text.ElideRight
            }
            Rectangle {
                x: 30
                y: parent.height - 72
                width: 168
                height: 43
                radius: 11
                color: infoPlayMouse.containsMouse ? "#E8E4F5" : "#F5F2FA"
                Text {
                    anchors.centerIn: parent
                    text: "▶  Reproducir"
                    color: "#18131F"
                    font.family: "Segoe UI"
                    font.pixelSize: 16
                    font.weight: Font.DemiBold
                }
                MouseArea {
                    id: infoPlayMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        novaHome.closeFeaturedInfo()
                        novaHome.playFeatured()
                    }
                }
            }
            Text {
                x: parent.width - 38
                y: 11
                text: "×"
                font.pixelSize: 27
                color: "#FFFFFF"
                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: novaHome.closeFeaturedInfo()
                }
            }
        }
    }
}
