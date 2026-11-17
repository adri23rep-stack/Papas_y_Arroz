#pragma once
// Experimental Nova <-> Veyra adapter, built only into a user's Veyra checkout.
// Licensed under GPL-3.0-or-later for compatibility with Veyra.
// Transport: newline-delimited JSON over the inherited Windows stdin/stdout pipes.
// The bridge never logs a media URL, token, or authentication header.
#include <QByteArray>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QScreen>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <windows.h>
#include "veyra/ui/QmlPlayerBridge.h"

class NovaStdioBridge final : public QObject {
public:
    NovaStdioBridge(veyra::ui::QmlPlayerBridge& player, QQuickWindow* window, QObject* parent)
        : QObject(parent), player_(player), window_(window) {
        if (qEnvironmentVariableIntValue("NOVA_VEYRA_STDIO") != 1) return;
        input_ = GetStdHandle(STD_INPUT_HANDLE);
        output_ = GetStdHandle(STD_OUTPUT_HANDLE);
        if (!input_ || !output_ || input_ == INVALID_HANDLE_VALUE ||
            output_ == INVALID_HANDLE_VALUE ||
            GetFileType(input_) != FILE_TYPE_PIPE ||
            GetFileType(output_) != FILE_TYPE_PIPE) return;
        enabled_ = true;
        connect(&inputTimer_, &QTimer::timeout, this, [this] { pollInput(); });
        inputTimer_.start(60);
        connect(&statusTimer_, &QTimer::timeout, this, [this] { status(); });
        statusTimer_.start(500);
        write({{"type", "nova-ready"}, {"protocol", 1}});
    }

private:
    void write(const QJsonObject& object) {
        if (!enabled_) return;
        const QByteArray data = QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
        DWORD written = 0;
        if (!WriteFile(output_, data.constData(), DWORD(data.size()), &written, nullptr)
            || written != DWORD(data.size())) {
            enabled_ = false;
            inputTimer_.stop();
            statusTimer_.stop();
        }
    }
    void status() {
        if (pendingResume_ > 0.0 && player_.running() && player_.duration() > 0.0) {
            player_.seekTo(pendingResume_);
            pendingResume_ = -1.0;
        }
        const double position = player_.position();
        const double duration = player_.duration();
        write({
            {"type", "nova-status"},
            {"protocol", 1},
            {"running", player_.running()},
            {"paused", player_.paused()},
            {"failed", player_.failed()},
            {"hasSource", player_.hasSource()},
            {"position", std::isfinite(position) ? (position > 0.0 ? position : 0.0) : 0.0},
            {"duration", std::isfinite(duration) ? (duration > 0.0 ? duration : 0.0) : 0.0},
            {"audioTracks", QJsonArray::fromVariantList(player_.audioTracks())},
            {"selectedAudioTrack", player_.selectedAudioTrack()},
            {"subtitleTracks", QJsonArray::fromVariantList(player_.subtitleTracks())},
            {"selectedSubtitle", player_.subtitlePrimary()},
            {"fgEnabled", player_.fgEnabled()},
            {"fgMultiplier", player_.fgMultiplier()},
            {"fgMaxMultiplier", player_.fgMaxMultiplier()},
            {"srEnabled", player_.srEnabled()},
            {"nrEnabled", player_.nrEnabled()},
            {"hdrEnabled", player_.videoHdr()},
            {"sourceFps", player_.sourceFps()},
            {"submitFps", player_.submitFps()},
            {"submitFpsKnown", player_.submitFpsKnown()}
        });
    }
    void execute(const QJsonObject& object) {
        if (object.value("protocol").toInt(-1) != 1) return;
        const QString cmd = object.value("cmd").toString();
        if (cmd == QLatin1String("open_source")) {
            const QString source = object.value("source").toString();
            if (source.isEmpty() || source.size() > 8192) return;
            const QUrl url(source, QUrl::StrictMode);
            if (url.isValid() && (url.scheme() == QLatin1String("http")
                                   || url.scheme() == QLatin1String("https"))) {
                if (url.host().isEmpty() || !url.userInfo().isEmpty() || url.hasFragment()) return;
                player_.openNetworkAddress(source);
            } else if (QFileInfo(source).isFile()) {
                player_.openPath(source);
            }
        } else if (cmd == QLatin1String("pause")) {
            player_.togglePlayPause();
        } else if (cmd == QLatin1String("stop")) {
            player_.stopPlayback();
        } else if (cmd == QLatin1String("seek_by")) {
            const double seconds = object.value("seconds").toDouble();
            if (std::isfinite(seconds) && std::abs(seconds) <= 600.0) player_.seekBy(seconds);
        } else if (cmd == QLatin1String("resume")) {
            const double seconds = object.value("seconds").toDouble();
            if (std::isfinite(seconds) && seconds > 0.0 && seconds < 86400.0)
                pendingResume_ = seconds;
        } else if (cmd == QLatin1String("volume")) {
            const double volume = object.value("value").toDouble(-1.0);
            if (std::isfinite(volume) && volume >= 0.0 && volume <= 1.0)
                player_.setVolume(volume);
        } else if (cmd == QLatin1String("speed")) {
            const double rate = object.value("value").toDouble(0.0);
            if (std::isfinite(rate) && rate >= player_.minimumPlaybackRate()
                && rate <= player_.maximumPlaybackRate()) player_.setPlaybackRate(rate);
        } else if (cmd == QLatin1String("audio_track")) {
            if (object.value("index").isDouble()) {
                const int index = object.value("index").toInt(-1);
                if (index >= 0) player_.setSelectedAudioTrack(index);
            }
        } else if (cmd == QLatin1String("subtitle_track")) {
            if (object.value("index").isDouble()) {
                const int index = object.value("index").toInt(-2);
                if (index == -1) {
                    player_.setSubtitlePrimary(-1);
                } else if (index >= 0) {
                    const QVariantList tracks = player_.subtitleTracks();
                    if (index < tracks.size() && tracks.at(index).toMap().value("usable").toBool())
                        player_.setSubtitlePrimary(index);
                }
            }
        } else if (cmd == QLatin1String("fullscreen")) {
            if (!window_) return;
            const bool enabled = object.contains("enabled") ?
                object.value("enabled").toBool() : !(window_->visibility() == QWindow::FullScreen);
            if (enabled) window_->showFullScreen();
            else window_->showNormal();
        } else if (cmd == QLatin1String("next_monitor")) {
            if (!window_) return;
            const auto screens = QGuiApplication::screens();
            if (screens.size() < 2) return;
            const int current = screens.indexOf(window_->screen());
            const int next = (current + 1) % screens.size();
            const bool fullscreen = (window_->visibility() == QWindow::FullScreen);
            if (fullscreen) window_->showNormal();
            window_->setScreen(screens.at(next));
            if (fullscreen) window_->showFullScreen();
            else {
                QRect geometry = window_->geometry();
                geometry.moveCenter(screens.at(next)->availableGeometry().center());
                window_->setGeometry(geometry);
            }
        } else if (cmd == QLatin1String("gpu_feature")) {
            // Explicit opt-in commands ONLY. Never change Veyra's saved preset
            // merely because Nova has launched.
            const QString feature = object.value("name").toString();
            const QJsonValue enabled = object.value("enabled");
            if (feature == QLatin1String("fg") && enabled.isBool())
                player_.setFgEnabled(enabled.toBool());
            else if (feature == QLatin1String("sr") && enabled.isBool())
                player_.setSrEnabled(enabled.toBool());
            else if (feature == QLatin1String("nr") && enabled.isBool())
                player_.setNrEnabled(enabled.toBool());
            else if (feature == QLatin1String("hdr") && enabled.isBool())
                player_.setVideoHdr(enabled.toBool());
        } else if (cmd == QLatin1String("fg_multiplier")) {
            const int n = object.value("value").toInt(-1);
            if (n >= 1 && n <= player_.fgMaxMultiplier()) player_.setFgMultiplier(n);
        }
    }
    void pollInput() {
        DWORD available = 0;
        if (!PeekNamedPipe(input_, nullptr, 0, nullptr, &available, nullptr) || !available) return;
        char block[4096];
        DWORD received = 0;
        const DWORD count = available < DWORD(sizeof(block)) ? available : DWORD(sizeof(block));
        if (!ReadFile(input_, block, count, &received, nullptr) || !received) return;
        buffer_.append(block, int(received));
        if (buffer_.size() > 8192) { buffer_.clear(); return; }
        for (;;) {
            const qsizetype newline = buffer_.indexOf('\n');
            if (newline < 0) break;
            const QByteArray line = buffer_.left(newline);
            buffer_.remove(0, newline + 1);
            QJsonParseError error{};
            const auto document = QJsonDocument::fromJson(line, &error);
            if (error.error == QJsonParseError::NoError && document.isObject())
                execute(document.object());
        }
    }
    veyra::ui::QmlPlayerBridge& player_;
    QQuickWindow* window_ = nullptr;
    HANDLE input_ = nullptr;
    HANDLE output_ = nullptr;
    bool enabled_ = false;
    double pendingResume_ = -1.0;
    QByteArray buffer_;
    QTimer inputTimer_;
    QTimer statusTimer_;
};
