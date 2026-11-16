import QtQuick

Item {
    id: nav
    width: parent ? parent.width : 1280
    height: searchOpen ? 107 : 63
    property bool searchOpen: false
    z: 18

    signal pairRequested()
    signal settingsRequested()
    property int selected: -1
    signal tabChosen(int index)
    function submitSearch() {
        if (searchInput.text.trim().length < 2) return
        novaHome.search(searchInput.text)
        searchOpen = false
    }
    function activateTab(index) {
        tabChosen(index)
        if (index === 4) {
            searchOpen = !searchOpen
            if (searchOpen) searchInput.forceActiveFocus()
            return
        }
        searchOpen = false
        if (index === 0) novaHome.home()
        else novaHome.openSection(index - 1)
    }
    Rectangle {
        width: parent.width
        height: 68
        gradient: Gradient {
            GradientStop { position: 0; color: "#EE0B0B0E" }
            GradientStop { position: 1; color: "#C50B0B0E" }
        }
    }
    Rectangle {
        id: pairButton
        objectName: "novaPairButton"
        anchors.right: parent.right
        anchors.rightMargin: 28
        y: 13
        width: 42
        height: 42
        radius: 12
        color: pairMouse.containsMouse || nav.selected === 6 ? "#37313F" : "#222227"
        border.width: nav.selected === 6 ? 2 : 0
        border.color: "#A78BFA"
        Accessible.role: Accessible.Button
        Accessible.name: "Vincular iPhone por QR o PIN"
        Accessible.onPressAction: nav.pairRequested()
        Canvas {
            anchors.centerIn: parent
            width: 21
            height: 21
            onPaint: {
                const ctx = getContext("2d")
                ctx.fillStyle = "#F4F3F7"
                for (const corner of [[0,0], [12,0], [0,12]]) {
                    ctx.fillRect(corner[0], corner[1], 9, 9)
                    ctx.clearRect(corner[0]+2, corner[1]+2, 5, 5)
                    ctx.fillRect(corner[0]+3, corner[1]+3, 3, 3)
                }
                ctx.fillRect(12,12,3,6); ctx.fillRect(15,15,6,3)
                ctx.fillRect(18,12,3,3); ctx.fillRect(12,18,6,3)
            }
        }
        MouseArea {
            id: pairMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: nav.pairRequested()
        }
    }
    Rectangle {
        id: settingsButton
        anchors.right: parent.right
        anchors.rightMargin: 82
        y: 13
        width: 42
        height: 42
        radius: 12
        color: settingsMouse.containsMouse || nav.selected === 5 ? "#37313F" : "#222227"
        border.width: nav.selected === 5 ? 2 : 0
        border.color: "#A78BFA"
        Accessible.role: Accessible.Button
        Accessible.name: "Ajustes de calidad de video"
        Accessible.onPressAction: nav.settingsRequested()
        Text {
            anchors.centerIn: parent
            text: "⚙"
            color: "#F4F3F7"
            font.pixelSize: 24
        }
        MouseArea {
            id: settingsMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: nav.settingsRequested()
        }
    }
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
                    opacity: (nav.selected >= 0 ? nav.selected === tab.index : tab.active) || tabMouse.containsMouse ? 1 : 0
                    color: "#A78BFA"
                    Behavior on opacity { NumberAnimation { duration: 180 } }
                }
                MouseArea {
                    id: tabMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: nav.activateTab(tab.index)
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
            onAccepted: nav.submitSearch()
        }
    }
}
