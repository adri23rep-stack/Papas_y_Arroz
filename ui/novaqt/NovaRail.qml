import QtQuick

Item {
    id: rail
    property string heading: ""
    property var entries: []
    property bool channel: false
    property int sectionId: -1
    property bool showBrowse: sectionId >= 0
    readonly property bool empty: !entries || entries.length === 0
    height: empty ? 90 : (channel ? 265 : 380)
    width: parent ? parent.width : 1280

    Text {
        id: headingText
        x: 53
        y: 0
        text: rail.heading
        font.family: "Segoe UI"
        font.pixelSize: 27
        font.weight: Font.DemiBold
        color: "#F4F3F7"
        renderType: Text.QtRendering
    }

    Text {
        x: rail.width - 145
        y: 7
        visible: rail.showBrowse
        text: "Ver todo   ›"
        font.family: "Segoe UI"
        font.pixelSize: 15
        color: browseMouse.containsMouse ? "#C4B5FD" : "#B3B1BD"
        Behavior on color { ColorAnimation { duration: 170 } }
        MouseArea {
            id: browseMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: novaHome.openSection(rail.sectionId)
        }
    }

    Text {
        x: 55
        y: 49
        width: rail.width - 110
        text: rail.sectionId === -1
            ? "Tus episodios pendientes aparecerán aquí cuando tengas progreso guardado."
            : novaHome.loading ? "Cargando catálogo..." : "No hay contenido en esta sección."
        color: "#85848F"
        font.family: "Segoe UI"
        font.pixelSize: 14
        visible: rail.empty
        elide: Text.ElideRight
    }

    ListView {
        id: strip
        x: 43
        y: 44
        width: Math.max(0,rail.width - 58)
        height: rail.channel ? 204 : 314
        model: rail.entries
        orientation: ListView.Horizontal
        spacing: rail.channel ? 4 : 5
        interactive: true
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        maximumFlickVelocity: 2200
        flickDeceleration: 4800
        cacheBuffer: 350
        currentIndex: rail.sectionId >= 0 && novaHome.selectedSection === rail.sectionId
            ? novaHome.selectedHomeCard : -1

        delegate: NovaCard {
            required property var modelData
            required property int index
            entry: modelData
            channel: rail.channel
            focused: rail.sectionId >= 0 &&
                novaHome.selectedSection === rail.sectionId &&
                novaHome.selectedHomeCard === index
            onActivated: {
                if(rail.sectionId === -1) novaHome.openContinue(index)
                else novaHome.openHomeCard(rail.sectionId,index)
            }
        }
        onCurrentIndexChanged: {
            if(currentIndex>=0 && count>0)
                positionViewAtIndex(currentIndex,ListView.Contain)
        }
        WheelHandler {
            target: null
            onWheel: (ev) => {
                const delta = ev.angleDelta.y !== 0 ? ev.angleDelta.y : ev.angleDelta.x
                strip.contentX = Math.max(0,Math.min(
                    Math.max(0,strip.contentWidth - strip.width),
                    strip.contentX - delta / 120 * 225))
                ev.accepted = true
            }
        }
    }
}
