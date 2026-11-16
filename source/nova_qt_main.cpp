#include "nova_qt_home.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTimer>
#include <cstdio>
#include <cstring>
#include <fstream>

int main(int argc,char** argv){
    const bool smoke=argc>1&&std::strcmp(argv[1],"--smoke")==0;
    auto checkpoint=[smoke](const char* message){
        if(!smoke)return;
        std::ofstream report("nova-qt-smoke.log",std::ios::app);
        report<<message<<'\n';
    };
    checkpoint("main_entered");
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QGuiApplication app(argc,argv);
    app.setApplicationName(QStringLiteral("Xuper TV Nova Qt"));
    // Share the same installation-neutral templates and artwork URLs.
    // Existing Windows/SDL player remains an untouched separate EXE.
    QDir::setCurrent(QCoreApplication::applicationDirPath());

    checkpoint("application_ready");
    NovaQtHome home;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("novaHome"),&home);
    checkpoint("qml_load_begin");
    engine.load(QUrl(QStringLiteral("qrc:/ui/novaqt/Main.qml")));
    checkpoint("qml_load_end");
    if(engine.rootObjects().isEmpty()){
        std::fputs("NOVA_QT: Could not load QML home.\n",stderr);
        return 2;
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
