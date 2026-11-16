import QtQuick

Item {
    id: card
    property var entry: ({})
    property bool channel: false
    property bool live: channel
    property double now: 0
    function epoch(value) {
        if (!value) return NaN
        if (/^\d{10}$/.test(value)) return Number(value) * 1000
        if (/^\d{13}$/.test(value)) return Number(value)
        return Date.parse(value)
    }
    readonly property var airing: {
        if (!live || !entry.schedule) return null
        let untimed = null
        for (const program of entry.schedule) {
            const start = epoch(program.start), end = epoch(program.end)
            if (start <= now && now < end) return {title: program.title, progress: (now-start)/(end-start)}
            if (!program.start && !program.end) untimed = {title: program.title, progress: -1}
        }
        return untimed
    }
    property bool focused: false
    property bool hovered: mouse.containsMouse
    property int artWidth: channel ? 224 : 166
    property int artHeight: channel ? 126 : 249
    readonly property bool active: hovered || focused
    width: artWidth + 22
    height: artHeight + (channel ? 72 : 52)
    z: active ? 2 : 0
    signal activated()

    Item {
        id: surface
        x: 11
        y: card.active ? 15 : 16
        width: card.artWidth
        height: card.artHeight + (card.channel ? 59 : 43)
        transformOrigin: Item.Center
        scale: card.active ? 1.08 : 1.0
        Behavior on scale {
            NumberAnimation { duration: 185; easing.type: Easing.OutCubic }
        }
        Behavior on y {
            NumberAnimation { duration: 185; easing.type: Easing.OutCubic }
        }

        Rectangle {
            id: shadow
            x: -3; y: 6
            width: card.artWidth + 6
            height: card.artHeight + 4
            radius: 16
            color: "#000000"
            opacity: card.active ? 0.65 : 0.20
            Behavior on opacity { NumberAnimation { duration: 185 } }
        }

        Rectangle {
            id: frame
            width: card.artWidth
            height: card.artHeight
            radius: 15
            antialiasing: true
            color: "#19191F"
            border.color: card.active ? "#A78BFA" : "#34343B"
            border.width: card.active ? 2 : 0
            Behavior on border.color { ColorAnimation { duration: 175 } }
        }

        Image {
            id: cover
            x: card.channel ? 28 : 2
            y: card.channel ? 21 : 2
            width: card.artWidth - 2*x
            height: card.artHeight - 2*y
            source: card.entry.artwork || ""
            fillMode: card.channel ? Image.PreserveAspectFit : Image.PreserveAspectCrop
            sourceSize.width: card.channel ? 440 : 500
            sourceSize.height: card.channel ? 248 : 750
            asynchronous: true
            smooth: true
            mipmap: true
            visible: !(logoCanvas.recolored && card.channel)
            onStatusChanged: { logoCanvas.recolored = false; logoCanvas.requestPaint() }
        }
        // Normalize opaque white-backed logos once at logo size. Transparent
        // marks and colorful artwork without a white background pass through.
        Canvas {
            id: logoCanvas
            property bool recolored: false
            property url logoUrl: card.channel ? (card.entry.artwork || "") : ""
            property url retainedUrl
            function loadLogo() {
                if (retainedUrl.toString() === logoUrl.toString()) return
                if (retainedUrl.toString()) unloadImage(retainedUrl)
                retainedUrl = logoUrl
                recolored = false
                if (retainedUrl.toString()) loadImage(retainedUrl)
                requestPaint()
            }
            onLogoUrlChanged: if (available) loadLogo()
            Component.onCompleted: if (available) loadLogo()
            onAvailableChanged: if (available) loadLogo()
            onImageLoaded: requestPaint()
            anchors.centerIn: cover
            width: Math.round(cover.paintedWidth)
            height: Math.round(cover.paintedHeight)
            visible: card.channel
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0, 0, width, height)
                if (card.channel && retainedUrl.toString() !== logoUrl.toString()) loadLogo()
                if (!card.channel || !isImageLoaded(logoUrl) || width < 4 || height < 4) return
                ctx.drawImage(logoUrl, 0, 0, width, height)
                const bitmap = ctx.getImageData(0, 0, width, height)
                const pixels = bitmap.data
                const corners = [0, width-1, width*(height-1), width*height-1]
                const white = corners.every(function(n) {
                    const i = 4*n
                    return Math.min(pixels[i],pixels[i+1],pixels[i+2],pixels[i+3]) >= 240
                })
                if (!white) { ctx.clearRect(0, 0, width, height); recolored = false; return }
                for (let i = 0; i < pixels.length; i += 4) {
                    const min = Math.min(pixels[i],pixels[i+1],pixels[i+2])
                    const max = Math.max(pixels[i],pixels[i+1],pixels[i+2])
                    if (max-min < 24) {
                        pixels[i+3] = Math.round(pixels[i+3]*(255-min)/255)
                        pixels[i] = 245; pixels[i+1] = 244; pixels[i+2] = 249
                    }
                }
                ctx.clearRect(0, 0, width, height)
                ctx.putImageData(bitmap, 0, 0, 0, 0, width, height)
                recolored = true
            }
        }
        // Cover the four cropped poster corners even with Qt's software renderer.
        Canvas {
            x: cover.x; y: cover.y
            width: cover.width; height: cover.height
            visible: !card.channel
            onPaint: {
                const ctx = getContext("2d")
                ctx.clearRect(0,0,width,height)
                ctx.fillStyle = "#0B0B0E"
                for (let i = 0; i < 4; ++i) {
                    ctx.save()
                    ctx.translate(i%2 ? width : 0, i>=2 ? height : 0)
                    ctx.scale(i%2 ? -1 : 1, i>=2 ? -1 : 1)
                    ctx.beginPath(); ctx.moveTo(0,0); ctx.lineTo(13,0)
                    ctx.arc(13,13,13,-Math.PI/2,-Math.PI,true)
                    ctx.closePath(); ctx.fill(); ctx.restore()
                }
            }
        }
        Text {
            anchors.centerIn: frame
            width: frame.width - 28
            visible: cover.status !== Image.Ready
            text: card.entry.title || ""
            maximumLineCount: 3
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            color: "#E4E1EB"
            font.pixelSize: card.channel ? 20 : 18
            font.weight: Font.DemiBold
        }

        Text {
            x: 4
            y: card.artHeight + 9
            width: card.artWidth - 8
            text: card.entry.title || ""
            color: card.active ? "#FFFFFF" : "#D9D7E0"
            font.family: "Segoe UI"
            font.pixelSize: card.channel ? 15 : 14
            font.weight: card.active ? Font.DemiBold : Font.Medium
            elide: Text.ElideRight
            maximumLineCount: 1
            Behavior on color { ColorAnimation { duration: 175 } }
        }
        Text {
            x: 4
            y: card.artHeight + 30
            width: card.artWidth - 8
            visible: card.live
            text: card.airing ? "Ahora en vivo · " + card.airing.title : "En vivo"
            color: "#8D8D99"
            font.family: "Segoe UI"
            font.pixelSize: 11
            elide: Text.ElideRight
        }
        Text {
            x: 4
            y: card.artHeight + 30
            width: card.artWidth - 8
            visible: !card.channel && typeof card.entry.resumeSeconds === "number" &&
                     card.entry.resumeSeconds > 5
            text: "REANUDAR · " + Math.floor(card.entry.resumeSeconds / 60) + " min"
            color: "#A78BFA"
            font.family: "Segoe UI"
            font.pixelSize: 11
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
        // Program progress is shown ONLY when the service gives real
        // scheduling data; no synthetic schedule/progress is fabricated.
        Rectangle {
            x: 4; y: card.artHeight + 47
            width: card.artWidth - 8
            height: 3
            radius: 2
            visible: card.airing !== null && card.airing.progress >= 0
            color: "#42424A"
            Rectangle {
                width: parent.width * Math.max(0,Math.min(1,card.airing ? card.airing.progress : 0))
                height: parent.height
                radius: 2
                color: "#A78BFA"
            }
        }
    }

    MouseArea {
        id: mouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: card.activated()
    }
}
