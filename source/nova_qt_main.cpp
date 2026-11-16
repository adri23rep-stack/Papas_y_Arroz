#include "nova_qt_home.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QQmlError>
#include <QQmlEngine>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
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
    if(argc>1 && std::strcmp(argv[1],"--smoke-video-fullscreen")==0){
        NovaQtPlayer player;
        xs::SourceResult source;
        source.ok=true;
        source.source="test.mp4";
        player.play(source,QStringLiteral("Nova fullscreen test"),false);
        QTimer poll;
        int stage=0, moving=0;
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
            if(moving<3)return;
            std::fprintf(stderr,"Moving video verified in stage %d\\n",stage);
            if(++stage==3){app.quit();return;}
            player.toggleFullscreen();
            previous=QImage();
            moving=0;
        });
        poll.start(250);
        QTimer::singleShot(25000,&app,[&]{app.exit(5);});
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
