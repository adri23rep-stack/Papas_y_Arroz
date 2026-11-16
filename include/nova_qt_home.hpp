#pragma once
#include "catalog.hpp"
#include "service.hpp"
#include "nova_qt_player.hpp"
#include "saved_catalog.hpp"
#include "playback_state.hpp"
#include <QObject>
#include <QVariantList>
#include <QString>
#include <QTimer>
#include <QQuickWindow>
#include <memory>
#include <optional>
#include <vector>

// The original Windows service and LAN pairing protocol remain authoritative.
// Qt only supplies the new catalog navigation, artwork and interaction layer.
class NovaQtHome : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList channels READ channels NOTIFY changed)
    Q_PROPERTY(QVariantList demoItems READ demoItems NOTIFY changed)
    Q_PROPERTY(bool catalogConnected READ catalogConnected NOTIFY changed)
    Q_PROPERTY(QVariantList series READ series NOTIFY changed)
    Q_PROPERTY(QVariantList movies READ movies NOTIFY changed)
    Q_PROPERTY(QVariantList items READ items NOTIFY changed)
    Q_PROPERTY(QVariantList continueWatching READ continueWatching NOTIFY changed)
    Q_PROPERTY(QString featuredTitle READ featuredTitle NOTIFY changed)
    Q_PROPERTY(QString featuredDescription READ featuredDescription NOTIFY changed)
    Q_PROPERTY(QString featuredBackdrop READ featuredBackdrop NOTIFY changed)
    Q_PROPERTY(bool featuredReady READ featuredReady NOTIFY changed)
    Q_PROPERTY(bool featuredInfoVisible READ featuredInfoVisible NOTIFY changed)
    Q_PROPERTY(QString pageTitle READ pageTitle NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(bool browsing READ browsing NOTIFY changed)
    Q_PROPERTY(int selectedItem READ selectedItem NOTIFY changed)
    Q_PROPERTY(int selectedSection READ selectedSection NOTIFY changed)
    Q_PROPERTY(int selectedHomeCard READ selectedHomeCard NOTIFY changed)
    Q_PROPERTY(QString pairingAddress READ pairingAddress NOTIFY changed)
    Q_PROPERTY(QString pairingPin READ pairingPin NOTIFY changed)
    Q_PROPERTY(QString pairingQr READ pairingQr NOTIFY changed)
    Q_PROPERTY(bool remoteReady READ remoteReady NOTIFY changed)
    Q_PROPERTY(bool remoteConnected READ remoteConnected NOTIFY changed)
    Q_PROPERTY(int brightness READ brightness NOTIFY changed)
    Q_PROPERTY(bool playing READ playing NOTIFY changed)
    Q_PROPERTY(QString playbackTitle READ playbackTitle NOTIFY changed)
    Q_PROPERTY(QString playbackNotice READ playbackNotice NOTIFY changed)
    Q_PROPERTY(QString playbackDiagnostics READ playbackDiagnostics NOTIFY changed)
    Q_PROPERTY(int videoPreset READ videoPreset NOTIFY changed)
    Q_PROPERTY(QString videoPresetLabel READ videoPresetLabel NOTIFY changed)
public:
    explicit NovaQtHome(QObject* parent=nullptr);
    ~NovaQtHome() override;
    const QVariantList& channels() const{return channels_;}
    const QVariantList& demoItems() const{return demo_items_;}
    bool catalogConnected() const{return catalog_connected_;}
    const QVariantList& series() const{return series_;}
    const QVariantList& movies() const{return movies_;}
    const QVariantList& items() const{return items_;}
    const QVariantList& continueWatching() const{return continue_watching_;}
    QString featuredTitle() const{return QString::fromStdString(featured_entry_.title);}
    QString featuredDescription() const{return QString::fromStdString(featured_entry_.description);}
    QString featuredBackdrop() const{return featured_backdrop_;}
    bool featuredReady() const{return !featured_entry_.title.empty();}
    bool featuredInfoVisible() const{return featured_info_visible_;}
    QString pageTitle() const{return page_title_;}
    QString status() const{return status_;}
    bool loading() const{return loading_;}
    bool browsing() const{return browsing_;}
    int selectedItem() const{return selected_item_;}
    int selectedSection() const{return selected_section_;}
    int selectedHomeCard() const{return selected_home_card_;}
    QString pairingAddress() const{return address_;}
    QString pairingPin() const{return pin_;}
    QString pairingQr() const{return qr_;}
    bool remoteReady() const{return remote_ready_;}
    bool remoteConnected() const{return remote_connected_;}
    int brightness() const{return brightness_;}
    bool playing() const{return player_.playing();}
    QString playbackTitle() const{return player_.title();}
    QString playbackNotice() const{return player_.notice();}
    QString playbackDiagnostics() const{return player_.diagnostics();}
    int videoPreset() const;
    QString videoPresetLabel() const;
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void playFeatured();
    Q_INVOKABLE void moreInfoFeatured(){featured_info_visible_=true;emit changed();}
    Q_INVOKABLE void closeFeaturedInfo(){featured_info_visible_=false;emit changed();}
    Q_INVOKABLE void openContinue(int index);
    Q_INVOKABLE void openDemo(int index);
    Q_INVOKABLE void openSection(int row);
    Q_INVOKABLE void openHomeCard(int row,int index);
    Q_INVOKABLE void selectGridItem(int index);
    Q_INVOKABLE void openItem(int index);
    Q_INVOKABLE void back();
    Q_INVOKABLE void home();
    Q_INVOKABLE void search(const QString& query);
    Q_INVOKABLE void stopPlayback(){player_.stop();}
    Q_INVOKABLE void togglePlayback(){player_.togglePause();}
    Q_INVOKABLE void setVideoPreset(int value);
    void setWindow(QQuickWindow* window){window_=window;}
signals:
    void changed();
    void directionPressed(int dx,int dy);
    void selectPressed();
    void backPressed();
private:
    struct History {xs::Catalog catalog;int selected=0;};
    void updateRemote();
    void refreshContinue();
    void updateFeatured();
    void pollRemote();
    void activate(const xs::Entry& entry);
    void browseEntry(const xs::Entry& entry);
    void playEntry(const xs::Entry& entry);
    void showCatalog(xs::Catalog catalog,bool pushHistory);
    void navigate(int dx,int dy);
    void clickPointer(bool down);
    QVariantList channels_,series_,movies_,items_,continue_watching_,demo_items_;
    std::vector<xs::Entry> continue_entries_;
    xs::Entry featured_entry_,active_entry_;
    std::optional<xs::Entry> queued_remote_entry_;
    QString featured_backdrop_;
    std::vector<xs::Entry> channel_entries_,series_entries_,movie_entries_,demo_entries_;
    xs::Catalog page_,root_;
    std::vector<History> history_;
    std::shared_ptr<xs::ServiceSession> session_;
    QTimer remote_timer_;
    QQuickWindow* window_=nullptr;
    NovaQtPlayer player_;
    xs::SavedCatalog saved_{"library_state.json"};
    xs::PlaybackHistory playback_history_{};
    QString page_title_,status_=QStringLiteral("Preparando catálogo...");
    QString address_,pin_,qr_;
    bool loading_=false,browsing_=false,remote_ready_=false,remote_connected_=false;
    bool catalog_connected_=false;
    bool featured_info_visible_=false;
    int selected_item_=0,selected_section_=0,selected_home_card_=0;
    int brightness_=100;
    bool sdl_started_=false;
};
