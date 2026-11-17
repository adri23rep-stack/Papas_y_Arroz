#include "NovaStdioBridge.hpp"
int main(int argc,char** argv){
    QGuiApplication app(argc,argv);
    veyra::ui::QmlPlayerBridge player;
    if(qEnvironmentVariable("NOVA_VEYRA_FIXTURE_MODE")=="unpatched")qunsetenv("NOVA_VEYRA_STDIO");
    NovaStdioBridge bridge(player,nullptr,&app);
    if(qEnvironmentVariable("NOVA_VEYRA_FIXTURE_MODE")=="failure")
        QTimer::singleShot(2000,&app,[&]{player.failure=true;});
    if(qEnvironmentVariable("NOVA_VEYRA_FIXTURE_MODE")=="crash")
        QTimer::singleShot(2000,&app,[&]{app.exit(7);});
    if(qEnvironmentVariable("NOVA_VEYRA_FIXTURE_MODE")=="ended")
        QTimer::singleShot(2000,&app,[&]{player.pos=120;player.active=false;player.setProperty("_novaPlaybackEnded",true);});
    QTimer::singleShot(30000,&app,[&]{app.exit(9);});
    return app.exec();
}
