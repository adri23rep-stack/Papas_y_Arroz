#include "nova_qt_home.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QTemporaryDir>
#include <QFile>
#include <QQmlError>
#include <QQmlEngine>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <QRegularExpression>
#include <QScreen>
#include <QImage>
#include <QPixmap>
#include <QSurfaceFormat>
#include <cstdio>
#include <cstring>
#include <fstream>

int main(int argc,char** argv){
    const bool smokeFullscreen=argc>1&&std::strcmp(argv[1],"--smoke-fullscreen")==0;
    const bool smoke=smokeFullscreen||
                     (argc>1&&std::strcmp(argv[1],"--smoke")==0);
    auto checkpoint=[smoke](const char* message){
        if(!smoke)return;
        std::ofstream report("nova-qt-smoke.log",std::ios::app);
        report<<message<<'\n';
    };
    checkpoint("main_entered");
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QGuiApplication app(argc,argv);
    // Request 4x multisampling on backends that support it. The QML
    // rounded-image masks also have their own antialiased render layers.
    QSurfaceFormat surface=QSurfaceFormat::defaultFormat();
    surface.setSamples(4);
    QSurfaceFormat::setDefaultFormat(surface);
    app.setApplicationName(QStringLiteral("Xuper TV Nova Qt"));
    // Share the same installation-neutral templates and artwork URLs.
    // Existing Windows/SDL player remains an untouched separate EXE.
    QDir::setCurrent(QCoreApplication::applicationDirPath());

    checkpoint("application_ready");
    if(smoke){
        static QTemporaryDir smokeSettings;
        if(!smokeSettings.isValid())return 21;
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,smokeSettings.path());
        {
            NovaQtPlayer player;
            if(!player.setVeyraSetting("veyra_fg",0) ||
               !player.setVeyraSetting("veyra_sr_quality",4) ||
               player.setVeyraSetting("veyra_hdr",9))return 22;
        }
        NovaQtPlayer reopened;
        const auto saved=reopened.veyraDefaults();
        if(saved.value("veyra_fg").toBool() || !saved.value("veyra_sr").toBool() ||
           saved.value("veyra_sr_quality").toInt()!=4 || saved.value("veyra_hdr").toBool())
            return 23;
        checkpoint("veyra_defaults_restored");
    }
    // Deterministic IPC integration test. Uses a fake child engine, never a GPU claim.
    if(argc>1 && std::strcmp(argv[1],"--smoke-veyra")==0){
        app.setQuitOnLastWindowClosed(false);
        NovaQtPlayer player;
        const QString mode=qEnvironmentVariable("NOVA_VEYRA_FIXTURE_MODE");
        const bool expectEnd=mode=="ended";
        const bool fallback=mode=="failure" || mode=="crash" || mode=="unpatched" || mode=="native-strict-protected";
        xs::SourceResult source;
        source.ok=true; source.source="test.mp4";
        if(mode=="native-strict-protected")source.headers={{"Content-Auth","synthetic-test-only"}};
        int stage=0;
        QObject::connect(&player,&NovaQtPlayer::finishedPlayback,&app,[&](double position,bool ended){
            if(expectEnd){app.exit(ended && position==120 ? 0 : 15);return;}
            if(!fallback){
                app.exit(stage==2 && position>=41 ? 0 : 11);
            }
        });
        QTimer poll;
        QObject::connect(&poll,&QTimer::timeout,&app,[&]{
            if(expectEnd)return;
            if(fallback){
                if(player.videoWindow() && !player.veyraActive()){
                    player.stop();
                    app.quit();
                }
                return;
            }
            if(!player.veyraActive())return;
            if(stage==0){
                if(player.videoWindow() || player.setVeyraSetting("veyra_multiplier",3)){
                    app.exit(12); return;
                }
                if(!player.setVeyraSetting("veyra_fg",1) ||
                   !player.setVeyraSetting("veyra_sr",1) ||
                   !player.setVeyraSetting("veyra_multiplier",4)){
                    app.exit(13); return;
                }
                player.togglePause();
                stage=1;
            }else if(stage==1){
                const auto state=player.veyraState();
                if(state.value("paused").toBool() && state.value("fgEnabled").toBool() &&
                   state.value("srEnabled").toBool() && state.value("fgMultiplier").toInt()==4 &&
                   state.value("position").toDouble()>=41){
                    stage=2;
                    player.stop();
                }
            }
        });
        player.play(source,QStringLiteral("Veyra IPC fixture"),false,42.0);
        poll.start(100);
        QTimer::singleShot(18000,&app,[&]{app.exit(14);});
        return app.exec();
    }
    const bool testRife=argc>1 && std::strcmp(argv[1],"--smoke-rife-fullscreen")==0;
    if(testRife || (argc>1 && std::strcmp(argv[1],"--smoke-video-fullscreen")==0)){
        NovaQtPlayer player;
        xs::SourceResult source;
        source.ok=true;
        source.source="test.mp4";
        player.play(source,QStringLiteral("Nova fullscreen test"),false);
        QTimer poll;
        int stage=0, moving=0;
        bool requested=false;
        QImage previous;
        QObject::connect(&poll,&QTimer::timeout,&app,[&]{
            auto* window=player.videoWindow();
            if(!window || !player.playing()){app.exit(4);return;}
            if((window->visibility()==QWindow::FullScreen)!=(stage==1))return;
            const auto size=window->size();
            const auto image=window->screen()->grabWindow(window->winId(),size.width()/2-64,
                size.height()/2-48,128,96).toImage();
            if(!image.isNull() && !previous.isNull() && image!=previous)++moving;
            previous=image;
            if(testRife && moving>=3 && !requested){
                player.requestQuality(4);
                if(!player.requestMotion(2)){app.exit(6);return;}
                requested=true;
                moving=0;
                return;
            }
            if(testRife){
                const QRegularExpression fps(QStringLiteral("FPS estimados tras filtro: ([0-9.]+)"));
                const double rate=fps.match(player.diagnostics()).captured(1).toDouble();
                if(rate<46 || rate>50)return;
            }
            if(moving<3)return;
            std::fprintf(stderr,"Moving video verified in stage %d\\n",stage);
            if(++stage==3){app.quit();return;}
            player.toggleFullscreen();
            previous=QImage();
            moving=0;
        });
        poll.start(250);
        QTimer::singleShot(testRife?90000:25000,&app,[&]{std::fprintf(stderr,"%s\\n",player.diagnostics().toUtf8().constData());app.exit(5);});
        return app.exec();
    }
    NovaQtHome home;
    QQmlApplicationEngine engine;
    // windeployqt puts importable modules beside this executable in /qml.
    // MSYS2's Qt may otherwise keep an absolute build-machine QML path.
    const QString portableQml=QDir(QCoreApplication::applicationDirPath())
                                  .absoluteFilePath(QStringLiteral("qml"));
    engine.addImportPath(portableQml);
    checkpoint(QFile::exists(QDir(portableQml).filePath(
               QStringLiteral("QtQuick/qmldir")))?
               "qtquick_module_present":"qtquick_module_missing");
    engine.rootContext()->setContextProperty(QStringLiteral("novaHome"),&home);
    QObject::connect(&engine,&QQmlEngine::warnings,&engine,
                     [smoke](const QList<QQmlError>& warnings){
        for(const auto& warning:warnings){
            const QByteArray message=warning.toString().toUtf8();
            std::fprintf(stderr,"NOVA_QML: %s\n",message.constData());
            if(smoke){
                std::ofstream report("nova-qt-smoke.log",std::ios::app);
                report<<"QML_ERROR "<<message.constData()<<'\n';
            }
        }
    });
    checkpoint(QFile::exists(QStringLiteral(":/ui/novaqt/Main.qml"))?
               "qml_resource_present":"qml_resource_missing");
    checkpoint("qml_load_begin");
    engine.load(QUrl(QStringLiteral("qrc:/ui/novaqt/Main.qml")));
    checkpoint("qml_load_end");
    if(engine.rootObjects().isEmpty()){
        std::fputs("NOVA_QT: Could not load QML home.\n",stderr);
        return 2;
    }
    auto* window=qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if(!window){
        std::fputs("NOVA_QT: QML did not create a native Qt Quick window.\n",stderr);
        return 2;
    }
    home.setWindow(window);
    if(smokeFullscreen){
        // Test responsive maximized-window layout without any real display.
        // The Qt offscreen platform provides a deterministic scene geometry.
        QObject* window=engine.rootObjects().front();
        window->setProperty("width",1280);
        window->setProperty("height",800);
        QCoreApplication::processEvents();
        const double compactRows=window->property("homeRowsTop").toDouble();
        window->setProperty("width",1920);
        window->setProperty("height",1080);
        QCoreApplication::processEvents();
        const double rowsTop=window->property("homeRowsTop").toDouble();
        const double blackFadeBottom=window->property("backdropBottom").toDouble();
        if(compactRows<450||compactRows>475||
           rowsTop<550||rowsTop>580||
           rowsTop-compactRows<90||
           blackFadeBottom<rowsTop+84||blackFadeBottom>rowsTop+86){
            std::fprintf(stderr,
                         "NOVA_FULLSCREEN_LAYOUT_FAIL compact=%.1f rows=%.1f fade=%.1f\n",
                         compactRows,rowsTop,blackFadeBottom);
            return 3;
        }
        QObject* featured=window->findChild<QObject*>(
            QStringLiteral("novaFeaturedTitle"));
        if(!featured||featured->property("text").toString().trimmed().isEmpty()||
           featured->property("text").toString()==QStringLiteral("XUPER TV")){
            std::fputs("NOVA_FEATURED_TITLE_FAIL\n",stderr);
            return 4;
        }
        checkpoint("fullscreen_layout_and_featured_title_passed");
    }
    if(smoke){
        QTimer::singleShot(1000,&app,[&app,&checkpoint](){
            checkpoint("timer_fired");
            std::puts("NOVA_QT_SMOKE_OK");
            app.quit();
        });
    }else home.refresh();
    checkpoint("event_loop_begin");
    const int code=app.exec();
    checkpoint("event_loop_end");
    return code;
}
