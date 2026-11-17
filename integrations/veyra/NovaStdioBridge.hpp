#pragma once
// Experimental Nova <-> Veyra adapter, built only into a user's Veyra checkout.
// Licensed under GPL-3.0-or-later for compatibility with Veyra.
// Transport: newline-delimited JSON over the inherited Windows stdin/stdout pipes.
// The bridge never logs a media URL, token, or authentication header.
#include <QByteArray>
#include <QFileInfo>
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
    NovaStdioBridge(veyra::ui::QmlPlayerBridge& player, QObject* parent)
        : QObject(parent), player_(player) {
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
            {"position", std::isfinite(position) ? std::max(0.0, position) : 0.0},
            {"duration", std::isfinite(duration) ? std::max(0.0, duration) : 0.0},
            {"audioTracks", QJsonArray::fromVariantList(player_.audioTracks())},
            {"selectedAudioTrack", player_.selectedAudioTrack()}
        });
    }
    void execute(const QJsonObject& object) {
        if (object.value("protocol").toInt(-1) != 1) return;
        const QString cmd = object.value("cmd").toString();
        if (cmd == QLatin1String("pause")) {
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
        }
    }
    void pollInput() {
        DWORD available = 0;
        if (!PeekNamedPipe(input_, nullptr, 0, nullptr, &available, nullptr) || !available) return;
        char block[4096];
        DWORD received = 0;
        const DWORD count = std::min<DWORD>(available, DWORD(sizeof(block)));
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
    HANDLE input_ = nullptr;
    HANDLE output_ = nullptr;
    bool enabled_ = false;
    double pendingResume_ = -1.0;
    QByteArray buffer_;
    QTimer inputTimer_;
    QTimer statusTimer_;
};
