import QtQuick
import QtQuick.Effects

Item {
    id: card
    property var entry: ({})
    property bool channel: false
    property bool focused: false
    property bool hovered: mouse.containsMouse
    property int artWidth: channel ? 216 : 166
    property int artHeight: channel ? 122 : 249
    readonly property bool active: hovered || focused
    width: artWidth + 22
    height: artHeight + (channel ? 72 : 52)
    z: active ? 2 : 0
    signal activated()

    Item {
        id: surface
        x: 11
        y: card.active ? 8 : 13
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
            border.width: card.active ? 1.7 : 0.7
            Behavior on border.color { ColorAnimation { duration: 175 } }
        }

        // TV cards are all letterboxed over the SAME dark surface:
        // white logo files don't create mismatched white card backgrounds.
        Image {
            id: cover
            x: card.channel ? 18 : 2
            y: card.channel ? 14 : 2
            width: card.artWidth - (card.channel ? 36 : 4)
            height: card.artHeight - (card.channel ? 28 : 4)
            source: card.entry.artwork || ""
            fillMode: card.channel ? Image.PreserveAspectFit : Image.PreserveAspectCrop
            sourceSize.width: card.channel ? 520 : 480
            sourceSize.height: card.channel ? 300 : 720
            asynchronous: true
            smooth: true
            mipmap: true
            visible: card.channel
        }

        Rectangle {
            id: coverMask
            x: 2; y: 2
            width: card.artWidth - 4
            height: card.artHeight - 4
            radius: 13
            color: "#FFFFFF"
            antialiasing: true
            visible: false
            layer.enabled: true
            layer.samples: 4
        }
        Image {
            id: poster
            x: 2; y: 2
            width: card.artWidth - 4
            height: card.artHeight - 4
            source: card.channel ? "" : (card.entry.artwork || "")
            fillMode: Image.PreserveAspectCrop
            sourceSize.width: 500
            sourceSize.height: 750
            asynchronous: true
            smooth: true
            mipmap: true
            visible: false
        }
        MultiEffect {
            x: 2; y: 2
            width: card.artWidth - 4
            height: card.artHeight - 4
            visible: !card.channel && poster.status === Image.Ready
            source: poster
            maskEnabled: true
            maskSource: coverMask
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
            visible: card.channel
            text: card.entry.epgTitle
                ? "Ahora: " + card.entry.epgTitle
                : "EN VIVO"
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
            visible: card.channel && card.entry.epgTitle &&
                     typeof card.entry.epgProgress === "number" &&
                     card.entry.epgProgress >= 0
            color: "#42424A"
            Rectangle {
                width: parent.width * Math.max(0,Math.min(1,card.entry.epgProgress || 0))
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
