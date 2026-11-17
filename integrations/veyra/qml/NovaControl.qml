// Nova experimental: zero-pixel remote control shim for a COPY of Veyra 2.1.9.
// Inserted as a child of the real Main.qml Window; never touches native D3D12.
import QtQuick
import "NovaBridgeConfig.js" as NovaConfig

Item {
    id: control
    visible: false
    width: 0
    height: 0
    property bool polling: false
    property bool posting: false
    property bool connected: false
    property int failedPolls: 0
    property real pendingResume: -1
    readonly property string endpoint: NovaConfig.endpoint || ""
    readonly property string token: NovaConfig.token || ""

    function url(route) {
        return endpoint + route + "?token=" + encodeURIComponent(token)
    }

    function execute(message) {
        if (!message || message.protocol !== 1) return
        var cmd = message.cmd || ""
        try {
            if (cmd === "open_source") {
                pendingResume = -1
                var source = message.source || ""
                if (!source || source.length > 8192) return
                if (source.indexOf("http://") === 0 || source.indexOf("https://") === 0)
                    veyra.openNetworkAddress(source)
                else veyra.openPath(source)
            } else if (cmd === "pause") veyra.togglePlayPause()
            else if (cmd === "stop") veyra.stopPlayback()
            else if (cmd === "seek_by") veyra.seekBy(Number(message.seconds || 0))
            else if (cmd === "resume") pendingResume = Number(message.seconds || 0)
            else if (cmd === "volume") veyra.setVolume(Number(message.value))
            else if (cmd === "speed") veyra.setPlaybackRate(Number(message.value))
            else if (cmd === "audio_track") veyra.setSelectedAudioTrack(Number(message.index))
            else if (cmd === "subtitle_track") veyra.setSubtitlePrimary(Number(message.index))
            else if (cmd === "fullscreen") {
                var hostWindow = control.Window.window
                if (hostWindow && hostWindow.toggleFullscreen)
                    hostWindow.toggleFullscreen()
            }
            else if (cmd === "gpu_feature") {
                if (message.name === "fg") veyra.setFgEnabled(Boolean(message.enabled))
                else if (message.name === "sr") veyra.setSrEnabled(Boolean(message.enabled))
                else if (message.name === "nr") veyra.setNrEnabled(Boolean(message.enabled))
                else if (message.name === "hdr") veyra.setVideoHdr(Boolean(message.enabled))
            } else if (cmd === "fg_multiplier") veyra.setFgMultiplier(Number(message.value))
            else if (cmd === "quit") Qt.quit()
        } catch (e) {
            // Never include source URL or other command parameters in diagnostics.
            console.warn("Nova QML bridge rejected a control command")
        }
    }

    Timer {
        interval: 160
        repeat: true
        running: control.endpoint.length > 0 && control.token.length > 0
        onTriggered: {
            if (control.polling) return
            control.polling = true
            var request = new XMLHttpRequest()
            request.onreadystatechange = function() {
                if (request.readyState !== 4) return
                control.polling = false
                if (request.status !== 200) {
                    if (control.connected && ++control.failedPolls >= 30)
                        Qt.quit() // The Nova parent/loopback proxy disappeared.
                    return
                }
                control.failedPolls = 0
                control.connected = true
                try { control.execute(JSON.parse(request.responseText)) }
                catch (e) { console.warn("Nova QML bridge invalid control reply") }
            }
            request.open("GET", control.url("/poll"), true)
            request.send()
        }
    }

    Timer {
        interval: 500
        repeat: true
        running: control.connected
        onTriggered: {
            if (control.posting) return
            if (control.pendingResume > 0 && veyra.running && veyra.duration > 0) {
                veyra.seekTo(control.pendingResume)
                control.pendingResume = -1
            }
            control.posting = true
            var status = {
                protocol: 1, type: "nova-status",
                running: veyra.running, paused: veyra.paused,
                failed: veyra.failed, ended: false,
                hasSource: veyra.hasSource,
                position: veyra.position, duration: veyra.duration,
                audioTracks: veyra.audioTracks,
                selectedAudioTrack: veyra.selectedAudioTrack,
                subtitleTracks: veyra.subtitleTracks,
                selectedSubtitle: veyra.subtitlePrimary,
                fgEnabled: veyra.fgEnabled,
                fgMultiplier: veyra.fgMultiplier,
                fgMultiplierChoices: veyra.fgMultiplierChoices,
                fgMaxMultiplier: veyra.fgMaxMultiplier,
                fgBackend: veyra.fgBackendName,
                srEnabled: veyra.srEnabled,
                nrEnabled: veyra.nrEnabled,
                hdrEnabled: veyra.videoHdr,
                sourceFps: veyra.sourceFps,
                submitFps: veyra.submitFps,
                submitFpsKnown: veyra.submitFpsKnown
            }
            var request = new XMLHttpRequest()
            request.onreadystatechange = function() {
                if (request.readyState === 4) control.posting = false
            }
            request.open("POST", control.url("/status"), true)
            request.setRequestHeader("Content-Type", "application/json")
            request.send(JSON.stringify(status))
        }
    }
}
