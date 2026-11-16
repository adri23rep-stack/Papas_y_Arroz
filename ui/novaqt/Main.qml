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

    // One focus path serves the keyboard and the paired iPhone's D-pad.
    property string zone: "hero"
    property int heroIndex: 0
    property int railIndex: 0
    property int cardIndex: 0
    property int tabIndex: 0
    property double clockMs: Date.now()
    property var rails: [demoRail, resumeRail, tvRail, seriesRail, movieRail]
    readonly property bool editingSearch: nav.searchOpen
    property bool inCatalog: novaHome.browsing
    onInCatalogChanged: {
        zone = inCatalog ? "grid" : "hero"
        nav.searchOpen = false
        page.contentY = 0
    }
    Timer {
        interval: 15000
        repeat: true
        running: root.visible
        onTriggered: root.clockMs = Date.now()
    }
    function revealRail() {
        const row = rails[railIndex]
        const top = rows.y + row.y - nav.height - 12
        const bottom = rows.y + row.y + row.height + 12
        page.contentY = Math.max(0, Math.min(page.contentHeight - page.height,
            Math.min(top, Math.max(page.contentY, bottom - page.height))))
    }
    function stepRail(from, step) {
        for (let i = from + step; i >= 0 && i < rails.length; i += step) {
            if (rails[i].visible && !rails[i].empty) {
                zone = "rail"
                railIndex = i
                cardIndex = 0
                revealRail()
                return
            }
        }
        if (step < 0) { zone = "hero"; page.contentY = 0 }
    }
    function move(dx, dy) {
        if (overlay.pairingVisible || novaHome.featuredInfoVisible || editingSearch) return
        if (zone === "nav") {
            tabIndex = Math.max(0, Math.min(5, tabIndex + dx))
            if (dy > 0) zone = inCatalog ? "grid" : "hero"
        } else if (inCatalog) {
            if (dy < 0 && overlay.atFirstRow) zone = "nav"
            else overlay.moveGrid(dx, dy)
        } else if (zone === "hero") {
            heroIndex = Math.max(0, Math.min(novaHome.featuredReady ? 1 : 0, heroIndex + dx))
            if (dy < 0) zone = "nav"
            if (dy > 0) stepRail(-1, 1)
        } else if (zone === "rail") {
            if (dy) stepRail(railIndex, dy > 0 ? 1 : -1)
            else cardIndex = Math.max(0, Math.min(rails[railIndex].entries.length - 1, cardIndex + dx))
        }
    }
    function choose() {
        if (overlay.pairingVisible) { overlay.pairingVisible = false; return }
        if (novaHome.featuredInfoVisible) {
            novaHome.closeFeaturedInfo()
            novaHome.playFeatured()
        } else if (editingSearch) nav.submitSearch()
        else if (zone === "nav") {
            if (tabIndex === 5) overlay.pairingVisible = true
            else nav.activateTab(tabIndex)
        } else if (inCatalog) novaHome.openItem(novaHome.selectedItem)
        else if (zone === "hero") hero.activate(heroIndex)
        else if (zone === "rail") rails[railIndex].openCard(cardIndex)
    }
    function goBack() {
        if (overlay.pairingVisible) overlay.pairingVisible = false
        else if (novaHome.featuredInfoVisible) novaHome.closeFeaturedInfo()
        else if (editingSearch) nav.searchOpen = false
        else { novaHome.back(); if (!inCatalog) { zone = "hero"; page.contentY = 0 } }
    }
    Connections {
        target: novaHome
        function onDirectionPressed(dx, dy) { root.move(dx, dy) }
        function onSelectPressed() { root.choose() }
        function onBackPressed() { root.goBack() }
    }
    Shortcut { sequence: "Left"; enabled: !root.editingSearch; onActivated: root.move(-1, 0) }
    Shortcut { sequence: "Right"; enabled: !root.editingSearch; onActivated: root.move(1, 0) }
    Shortcut { sequence: "Up"; enabled: !root.editingSearch; onActivated: root.move(0, -1) }
    Shortcut { sequence: "Down"; enabled: !root.editingSearch; onActivated: root.move(0, 1) }
    Shortcut { sequence: "Return"; enabled: !root.editingSearch; onActivated: root.choose() }
    Shortcut { sequence: "Enter"; enabled: !root.editingSearch; onActivated: root.choose() }
    Shortcut { sequence: "Escape"; onActivated: root.goBack() }

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
            selectedAction: root.zone === "hero" ? root.heroIndex : -1
            railTop: root.homeRowsTop
        }
        Column {
            id: rows
            x: 0
            y: root.homeRowsTop
            width: page.width
            spacing: 32

            NovaRail {
                id: demoRail
                selected: root.zone === "rail" && root.railIndex === 0 ? root.cardIndex : -1
                now: root.clockMs
                width: rows.width
                heading: "Prueba local del reproductor (no es TV en vivo)"
                entries: novaHome.demoItems
                sectionId: -1
                demo: true
                channel: true
                visible: !novaHome.catalogConnected && !novaHome.loading &&
                         novaHome.demoItems.length > 0
            }
            NovaRail {
                id: resumeRail
                selected: root.zone === "rail" && root.railIndex === 1 ? root.cardIndex : -1
                now: root.clockMs
                width: rows.width
                heading: "Continuar viendo"
                entries: novaHome.continueWatching
                sectionId: -1
                channel: false
                visible: novaHome.catalogConnected
            }
            NovaRail {
                id: tvRail
                selected: root.zone === "rail" && root.railIndex === 2 ? root.cardIndex : -1
                now: root.clockMs
                width: rows.width
                heading: "TV en vivo"
                entries: novaHome.channels
                sectionId: 0
                channel: true
                visible: novaHome.catalogConnected
            }
            NovaRail {
                id: seriesRail
                selected: root.zone === "rail" && root.railIndex === 3 ? root.cardIndex : -1
                now: root.clockMs
                width: rows.width
                heading: "Series"
                entries: novaHome.series
                sectionId: 1
                channel: false
                visible: novaHome.catalogConnected
            }
            NovaRail {
                id: movieRail
                selected: root.zone === "rail" && root.railIndex === 4 ? root.cardIndex : -1
                now: root.clockMs
                width: rows.width
                heading: "Películas"
                entries: novaHome.movies
                sectionId: 2
                channel: false
                visible: novaHome.catalogConnected
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
        id: overlay
        now: root.clockMs
        gridFocused: root.zone === "grid"
        z: pairingVisible ? 48 : 33
    }
    NovaTopNav {
        anchors.top: parent.top
        width: parent.width
        id: nav
        onPairRequested: { searchOpen = false; overlay.pairingVisible = true }
        selected: root.zone === "nav" ? root.tabIndex : -1
        onTabChosen: (index) => {
            root.tabIndex = index
            if (index === 0) { root.zone = "hero"; page.contentY = 0 }
            else if (index < 4) root.zone = "grid"
        }
        visible: !overlay.pairingVisible && !novaHome.featuredInfoVisible
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
