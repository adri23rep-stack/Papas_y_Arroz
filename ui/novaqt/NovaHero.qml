import QtQuick

Item {
    id: hero
    property int selectedAction: -1
    function activate(index) {
        if (novaHome.loading) return
        if (!novaHome.featuredReady) novaHome.refresh()
        else if (index === 1) novaHome.moreInfoFeatured()
        else novaHome.playFeatured()
    }
    property real railTop: 460
    height: railTop + 85
    width: parent ? parent.width : 1280

    Rectangle {
        anchors.fill: parent
        color: "#0B0B0E"
    }
    Image {
        id: backdrop
        // Portrait-only titles keep a portrait on the right. Never enlarge
        // a catalog thumbnail into a full-screen landscape.
        readonly property bool wide: implicitWidth >= 1200 && implicitWidth >= implicitHeight * 1.45
        anchors.right: parent.right
        anchors.rightMargin: wide ? 0 : parent.width * 0.09
        anchors.verticalCenter: parent.verticalCenter
        width: wide ? parent.width : Math.min(parent.width * 0.36, implicitWidth)
        height: wide ? parent.height : Math.min(parent.height * 0.83, implicitHeight)
        source: novaHome.featuredBackdrop
        fillMode: wide ? Image.PreserveAspectCrop : Image.PreserveAspectFit
        asynchronous: true
        cache: true
        smooth: true
        mipmap: true
        visible: novaHome.featuredReady
    }
    // Fade from image to almost-black at the LEFT where the metadata lives.
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#F208080B" }
            GradientStop { position: 0.35; color: "#B508080B" }
            GradientStop { position: 0.64; color: "#4208080B" }
            GradientStop { position: 1.0; color: "#0508080B" }
        }
    }
    // Fade into the browse rails BELOW. Never end on a bright blue strip.
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#080B0B0E" }
            GradientStop { position: 0.44; color: "#080B0B0E" }
            GradientStop { position: 0.72; color: "#660B0B0E" }
            GradientStop { position: 0.89; color: "#E20B0B0E" }
            GradientStop { position: 1.0; color: "#FF0B0B0E" }
        }
    }
    Text {
        x: 55
        anchors.bottom: featuredTitle.top
        anchors.bottomMargin: 8
        width: Math.min(710,hero.width - 110)
        text: novaHome.featuredReady ? "DESTACADO" :
              novaHome.loading ? "CONECTANDO" :
              novaHome.catalogConnected ? "TU CATÁLOGO" : "CONEXIÓN PENDIENTE"
        color: "#BBA4FB"
        font.family: "Segoe UI"
        font.pixelSize: 12
        font.weight: Font.Bold
        font.letterSpacing: 2
    }

    Text {
        id: featuredTitle
        objectName: "novaFeaturedTitle"
        x: 53
        anchors.bottom: synopsis.top
        anchors.bottomMargin: 10
        width: Math.min(710, hero.width * 0.62)
        text: novaHome.featuredReady ? novaHome.featuredTitle :
              novaHome.catalogConnected ? "Explora tu catálogo" : "Catálogo desconectado"
        font.family: "Segoe UI"
        font.pixelSize: hero.width > 1640 ? 54 : 45
        font.weight: Font.Bold
        color: "#FFFFFF"
        wrapMode: Text.WordWrap
        maximumLineCount: 2
        elide: Text.ElideRight
        renderType: Text.QtRendering
        style: Text.Raised
        styleColor: "#96000000"
    }
    Text {
        id: synopsis
        x: 56
        y: Math.max(248,hero.railTop - 132)
        width: Math.min(560,hero.width * 0.55)
        height: 53
        text: novaHome.featuredReady
            ? (novaHome.featuredDescription || "Sin sinopsis disponible en el catálogo.")
            : novaHome.loading ? "Conectando al servicio y consultando tu contenido..."
            : novaHome.status
        color: "#DDDDE3"
        font.family: "Segoe UI"
        font.pixelSize: 16
        lineHeight: 1.2
        maximumLineCount: 2
        wrapMode: Text.WordWrap
        elide: Text.ElideRight
        renderType: Text.QtRendering
    }
    Row {
        x: 56
        y: Math.max(319,hero.railTop - 67)
        spacing: 13
        Rectangle {
            border.width: hero.selectedAction === 0 ? 3 : 0
            border.color: "#A78BFA"
            width: 194
            height: 50
            radius: 12
            antialiasing: true
            color: novaHome.loading ? "#4A4850" :
                   playMouse.containsMouse ? "#E6DDFB" : "#F4F2FA"
            scale: playMouse.containsMouse ? 1.035 : 1
            Behavior on scale {
                NumberAnimation { duration: 180; easing.type: Easing.OutCubic }
            }
            Text {
                anchors.centerIn: parent
                text: novaHome.featuredReady ? "▶︎  Reproducir" : "↻  Reintentar"
                color: "#17131D"
                font.family: "Segoe UI"
                font.pixelSize: 17
                font.weight: Font.Bold
            }
            MouseArea {
                id: playMouse
                anchors.fill: parent
                hoverEnabled: true
                enabled: !novaHome.loading
                cursorShape: Qt.PointingHandCursor
                onClicked: {
                    if(novaHome.featuredReady) novaHome.playFeatured()
                    else novaHome.refresh()
                }
            }
        }
        Rectangle {
            visible: novaHome.featuredReady
            width: 144
            height: 50
            radius: 12
            antialiasing: true
            color: moreMouse.containsMouse ? "#4E475B" : "#A52B2933"
            border.width: hero.selectedAction === 1 ? 2 : 0
            border.color: "#A78BFA"
            Behavior on color { ColorAnimation { duration: 170 } }
            Text {
                anchors.centerIn: parent
                text: "ⓘ  Más info"
                color: "#FFFFFF"
                font.family: "Segoe UI"
                font.pixelSize: 16
                font.weight: Font.DemiBold
            }
            MouseArea {
                id: moreMouse
                anchors.fill: parent
                hoverEnabled: true
                enabled: novaHome.featuredReady
                cursorShape: Qt.PointingHandCursor
                onClicked: novaHome.moreInfoFeatured()
            }
        }
    }
}
