#pragma once
// Transport fixture, not a renderer. The separate MSVC job checks the real API.
#include <QString>
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
namespace veyra::ui {
class QmlPlayerBridge : public QObject {
public:
    bool active=false, pause=false, failure=false, fg=false, sr=false, nr=false, hdr=false;
    double pos=0, length=120, gain=1, rate=1;
    int multiplier=2, audio=7, subtitle=-1;
    bool running() const{return active;}
    bool paused() const{return pause;}
    bool failed() const{return failure;}
    bool hasSource() const{return active;}
    double position() const{return pos;}
    double duration() const{return length;}
    QVariantList audioTracks() const{return {QVariantMap{{"index",7},{"label","Audio test"}}};}
    int selectedAudioTrack() const{return audio;}
    QVariantList subtitleTracks() const{return {QVariantMap{{"index",0},{"label","Subtitle test"},{"usable",true}},QVariantMap{{"index",1},{"label","Unsupported"},{"usable",false}}};}
    int subtitlePrimary() const{return subtitle;}
    bool fgEnabled() const{return fg;}
    int fgMultiplier() const{return multiplier;}
    int fgMaxMultiplier() const{return 4;}
    QVariantList fgMultiplierChoices() const{return {QVariantMap{{"id","2"},{"label","2X"}},QVariantMap{{"id","4"},{"label","4X"}}};}
    QString fgBackendName() const{return "fixture";}
    bool srEnabled() const{return sr;}
    bool nrEnabled() const{return nr;}
    bool videoHdr() const{return hdr;}
    double sourceFps() const{return 24;}
    double submitFps() const{return fg?24*multiplier:24;}
    bool submitFpsKnown() const{return true;}
    bool openNetworkAddress(const QString&){active=true;pos=0;return true;}
    void openPath(const QString&){active=true;pos=0;}
    void togglePlayPause(){pause=!pause;}
    void stopPlayback(){active=false;}
    void seekBy(double n){pos+=n;}
    void seekTo(double n){pos=n;}
    void setVolume(double n){gain=n;}
    double minimumPlaybackRate() const{return .5;}
    double maximumPlaybackRate() const{return 2;}
    void setPlaybackRate(double n){rate=n;}
    void setSelectedAudioTrack(int n){audio=n;}
    void setSubtitlePrimary(int n){subtitle=n;}
    void setFgEnabled(bool n){fg=n;}
    void setSrEnabled(bool n){sr=n;}
    void setNrEnabled(bool n){nr=n;}
    void setVideoHdr(bool n){hdr=n;}
    void setFgMultiplier(int n){multiplier=n;}
};
}
