#pragma once
#include "service.hpp"
#include <QObject>
#include <QMutex>
#include <QString>
#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
struct mpv_handle;

// Keep Qt Quick rendering and media playback separate. libmpv owns its video
// window while QML remains alive, and remote actions use the same player.
class NovaQtPlayer : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool playing READ playing NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString notice READ notice NOTIFY changed)
public:
    explicit NovaQtPlayer(QObject* parent=nullptr);
    ~NovaQtPlayer() override;
    bool playing() const {return playing_.load();}
    QString title() const {return title_;}
    QString notice() const {return notice_;}
    QString diagnostics() const {return diagnostics_;}
    void play(const xs::SourceResult& result,const QString& title,bool live,double resumeSeconds=0);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void setVolume(int value);
    Q_INVOKABLE void setSpeed(int percent);
    Q_INVOKABLE void seek(double seconds);
    void applySetting(int id,int value);
    // Quality requests are picked up and debounced by the running mpv worker.
    void requestQuality(int value);
    bool requestMotion(int value);
    static bool gpuRifeReady();
    void toggleFullscreen();
    void selectTrack(int kind,int id,uint32_t generation);
signals:
    void changed();
    void finishedPlayback(double seconds,bool ended);
private:
    void command(const char* name,const char* argument);
    void finish(const QString& message);
    void playWorker(xs::SourceResult result,QString title,bool live,double resumeSeconds);
    std::thread thread_;
    mutable QMutex mutex_;
    mpv_handle* handle_=nullptr;
    std::atomic<bool> playing_{false},stop_{false};
    std::atomic<int> requested_quality_{1}, requested_motion_{0};
    std::atomic<std::uint64_t> restart_revision_{0};
    QString title_,notice_,diagnostics_;
};
