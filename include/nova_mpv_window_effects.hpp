#pragma once
#include "nova_veyra_native_loader.hpp"
#include <QGuiApplication>
#include <QElapsedTimer>
#include <QTimer>
#include <QWindow>
#include <QKeyEvent>
#include <cstdio>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

// WGC takes only Nova's own marked MPV window. Video never travels through IPC.
class NovaEffectWindow : public QWindow {
    void keyPressEvent(QKeyEvent* event) override {
        switch(event->key()){
        case Qt::Key_Space: std::puts("pause");std::fflush(stdout);break;
        case Qt::Key_Left: std::puts("back");std::fflush(stdout);break;
        case Qt::Key_Right: std::puts("forward");std::fflush(stdout);break;
        case Qt::Key_Escape: if(visibility()==FullScreen)showNormal();else close();break;
        case Qt::Key_F: case Qt::Key_F11: toggleFull();break;
        default: QWindow::keyPressEvent(event);
        }
    }
    void mouseDoubleClickEvent(QMouseEvent*) override {toggleFull();}
    void toggleFull(){if(visibility()==FullScreen)showNormal();else showFullScreen();}
};

inline int novaMpvWindowEffects(QGuiApplication& app){
#ifdef Q_OS_WIN
    const auto args=app.arguments();
    if(args.size()!=8)return 2;
    const auto id=args[2].toULongLong();
    const auto source=reinterpret_cast<HWND>(quintptr(id));
    const auto owner=args[3].toUInt();
    const auto mark=args[4].toULongLong();
    const auto ownsSource=[=]{
        DWORD pid=0;
        GetWindowThreadProcessId(source,&pid);
        return pid && pid==owner && pid!=GetCurrentProcessId() && mark &&
            quintptr(GetPropW(source,L"NovaMpvEffects"))==mark;
    };
    if(!ownsSource())return 3;
    NovaEffectWindow output;
    output.setTitle(QStringLiteral("MPV + Veyra · preparando imagen"));
    RECT bounds{};
    if(GetWindowRect(source,&bounds))output.resize(qBound(320,int(bounds.right-bounds.left),1920),
                                                    qBound(180,int(bounds.bottom-bounds.top),1080));
    NovaVeyraNativeLoader engine;
    if(!engine.load(qEnvironmentVariable("NOVA_VEYRA_NATIVE_DLL")))return 4;
    NovaVeyraSettings options{};
    options.size=sizeof(options);
    options.fg_backend=NOVA_VEYRA_FG_VFG;
    options.fg_multiplier=2;
    options.sr_quality=args[7].toUInt();
    const auto features=args[6].toUInt();
    options.features=features & ~NOVA_VEYRA_FEATURE_FG;
    const int fps=qBound(1,args[5].toInt(),240);
    const QString input=QStringLiteral("screen:0:%1:0:%2:0:0:0:0:0").arg(id).arg(fps);
    output.show();
    if(!engine.open(reinterpret_cast<void*>(output.winId()),input,options))return 5;
    QElapsedTimer startup;startup.start();
    QTimer poll;
    bool requested=false;
    QObject::connect(&poll,&QTimer::timeout,&app,[&]{
        NovaVeyraSnapshot state{};
        if(!ownsSource() || !engine.snapshot(state) || state.failed ||
           state.transport==NOVA_VEYRA_ENDED || (!state.source_frames && startup.elapsed()>20000)){
            app.exit(6);return;
        }
        if(!requested && state.source_frames>=10 && (features & NOVA_VEYRA_FEATURE_FG)){
            requested=true;
            engine.change(NOVA_VEYRA_FEATURE_FG,true);
        }
        NovaVeyraFlowStats flow{};
        if(engine.flowSnapshot(flow) && flow.session_id==state.session_id){
            output.setTitle(QStringLiteral("MPV + Veyra · %1 FPS DXGI · %2 generados · SR %3")
                .arg(flow.present_submit_fps,0,'f',1).arg(qulonglong(flow.generated_submitted))
                .arg(state.sr_active?"activo":"inactivo"));
            std::printf("flow %.1f %llu %u\n",flow.present_submit_fps,
                static_cast<unsigned long long>(flow.generated_submitted),unsigned(state.sr_active!=0));
            std::fflush(stdout);
        }
    });
    poll.start(500);
    return app.exec();
#else
    Q_UNUSED(app);
    return 3;
#endif
}
