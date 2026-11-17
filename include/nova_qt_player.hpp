#pragma once
#include "service.hpp"
#include <QObject>
#include <QWindow>
#include <QProcess>
#include <QByteArray>
#include <QVariantMap>
#include <QElapsedTimer>
#include <QTimer>
#include <functional>
class QJsonObject;
#include <QMutex>
#include <QString>
#include <atomic>
#include <cstdint>
#include <memory>
#include <thread>
struct mpv_handle;

// Qt owns the top-level video window; mpv draws into its native child.
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
    bool veyraActive() const {return veyraBridgeReady_ && !veyraState_.isEmpty();}
    QVariantMap veyraState() const {return veyraState_;}
    bool setVeyraSetting(const QString& key,int value);
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
    void cycleMonitor();
    QWindow* videoWindow() const { return videoWindow_.get(); }
    void selectTrack(int kind,int id,uint32_t generation);
signals:
    void changed();
    void finishedPlayback(double seconds,bool ended);
private:
    bool eventFilter(QObject* object,QEvent* event) override;
    std::unique_ptr<QWindow> videoWindow_;
    std::unique_ptr<QProcess> veyraProcess_;
    QByteArray veyraOutput_;
    QString veyraSource_;
    QVariantMap veyraState_;
    QTimer veyraWatchdog_;
    QElapsedTimer veyraClock_;
    qint64 veyraLastStatus_=0;
    bool veyraStarted_=false;
    bool veyraEnded_=false;
    bool veyraFallbackPending_=false;
    std::function<void()> veyraFallback_;
    void scheduleVeyraFallback();
    bool skipVeyraOnce_=false;
    bool veyraStopRequested_=false;
    std::uint64_t veyraAttemptId_=0;
    bool veyraBridgeReady_=false;
    bool veyraLive_=false;
    double veyraLastPosition_=-1.0;
    double veyraResumeSeconds_=0.0;
    void sendVeyraControl(const QJsonObject& command);
    bool restoreMaximized_=false;
    std::int64_t videoWindowId_=0;
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
