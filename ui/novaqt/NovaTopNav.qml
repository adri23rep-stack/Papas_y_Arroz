import QtQuick

Item {
    id: nav
    width: parent ? parent.width : 1280
    height: searchOpen ? 107 : 63
    property bool searchOpen: false
    z: 18

    Row {
        x: 53
        y: 22
        spacing: 30
        Repeater {
            model: ["Inicio", "TV", "Series", "Películas", "Buscar"]
            delegate: Item {
                id: tab
                required property string modelData
                required property int index
                width: title.implicitWidth + 5
                height: 31
                property bool active:
                    index === 0 ? !novaHome.browsing :
                    index === 1 ? novaHome.browsing && novaHome.pageTitle === "TV en vivo" :
                    index === 2 ? novaHome.browsing && novaHome.pageTitle === "Series" :
                    index === 3 ? novaHome.browsing && novaHome.pageTitle === "Películas" :
                    nav.searchOpen
                Text {
                    id: title
                    text: tab.modelData
                    color: tab.active || tabMouse.containsMouse ? "#FFFFFF" : "#BAB8C1"
                    font.family: "Segoe UI"
                    font.pixelSize: 15
                    font.weight: tab.active ? Font.DemiBold : Font.Normal
                    Behavior on color { ColorAnimation { duration: 180 } }
                }
                Rectangle {
                    width: title.implicitWidth
                    height: 2
                    y: 29
                    radius: 1
                    opacity: tab.active || tabMouse.containsMouse ? 1 : 0
                    color: "#A78BFA"
                    Behavior on opacity { NumberAnimation { duration: 180 } }
                }
                MouseArea {
                    id: tabMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if(tab.index === 0) novaHome.home()
                        else if(tab.index === 1) novaHome.openSection(0)
                        else if(tab.index === 2) novaHome.openSection(1)
                        else if(tab.index === 3) novaHome.openSection(2)
                        else {
                            nav.searchOpen = !nav.searchOpen
                            if(nav.searchOpen) searchInput.forceActiveFocus()
                        }
                    }
                }
            }
        }
    }
    Rectangle {
        x: 53; y: 63
        width: Math.min(460,Math.max(280,nav.width - 150))
        height: 41
        radius: 11
        color: "#E922222A"
        border.color: searchInput.activeFocus ? "#A78BFA" : "#4B4858"
        visible: nav.searchOpen
        Text {
            x: 16
            anchors.verticalCenter: parent.verticalCenter
            text: "Buscar película, serie o canal..."
            visible: !searchInput.text.length
            color: "#9996A4"
            font.pixelSize: 14
            font.family: "Segoe UI"
        }
        TextInput {
            id: searchInput
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            verticalAlignment: TextInput.AlignVCenter
            color: "#FFFFFF"
            font.family: "Segoe UI"
            font.pixelSize: 15
            clip: true
            selectByMouse: true
            onAccepted: {
                novaHome.search(text)
                nav.searchOpen = false
            }
        }
    }
}
