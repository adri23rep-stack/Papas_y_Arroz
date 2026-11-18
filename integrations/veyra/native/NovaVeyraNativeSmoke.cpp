// SPDX-License-Identifier: GPL-3.0-only
// Native GPU hardware smoke for the ORIGINAL Veyra EngineController.
// Does not run QML, Python, MPV, or a substituted software renderer.
// Requires a real MP4, native MSVC DLL, original shaders and RTX hardware.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
#include <cwchar>
#include <iostream>
#include <string>
#include "NovaVeyraApi.h"

namespace {
LRESULT CALLBACK windowProc(HWND w, UINT msg, WPARAM wp, LPARAM lp){
    if(msg==WM_DESTROY){PostQuitMessage(0);return 0;}
    return DefWindowProcW(w,msg,wp,lp);
}
void pumpWindow(){
    MSG msg{};
    while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){
        TranslateMessage(&msg);DispatchMessageW(&msg);
    }
}
void printState(const NovaVeyraSnapshot& s){
    std::wcout << L"transport=" << s.transport
               << L" running=" << s.running
               << L" failed=" << s.failed
               << L" frames=" << s.source_frames
               << L" generated=" << s.generated_frames
               << L" fps_source=" << s.source_fps
               << L" fps_submitted=" << (s.submit_fps_known?s.submit_fps:0.0)
               << L" fg_active=" << s.fg_active
               << L" fg_backend=" << s.applied_fg_backend
               << L" sr_active=" << s.sr_active << L'\n';
}
}

int wmain(int argc,wchar_t** argv){
    if(argc<2 || argc>3 || (argc==3 && wcscmp(argv[2],L"--fg2x")!=0)){
        std::wcerr << L"Usage: nova_veyra_native_smoke.exe <local-test.mp4> [--fg2x]\n";
        return 2;
    }
    // Strictly local source; this smoke never transmits URLs or user secrets.
    const DWORD attrs=GetFileAttributesW(argv[1]);
    if(attrs==INVALID_FILE_ATTRIBUTES || (attrs&FILE_ATTRIBUTE_DIRECTORY)){
        std::wcerr << L"FAIL: Missing readable local test file.\n";return 3;
    }
    if(nova_veyra_abi()!=NOVA_VEYRA_NATIVE_ABI){
        std::wcerr << L"FAIL: Incompatible native Veyra ABI.\n";return 4;
    }
    HINSTANCE inst=GetModuleHandleW(nullptr);
    WNDCLASSW klass{};
    klass.lpfnWndProc=windowProc;
    klass.hInstance=inst;
    klass.lpszClassName=L"NovaVeyraGPUAlphaSmoke";
    if(!RegisterClassW(&klass) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS){
        std::wcerr<<L"FAIL: Could not register video window.\n";return 5;
    }
    HWND hwnd=CreateWindowExW(0,klass.lpszClassName,L"Nova Veyra GPU Test",
              WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,960,540,
              nullptr,nullptr,inst,nullptr);
    if(!hwnd){std::wcerr<<L"FAIL: Could not create native video window.\n";return 6;}
    ShowWindow(hwnd,SW_SHOW);
    UpdateWindow(hwnd);
    void* engine=nova_veyra_create();
    if(!engine){DestroyWindow(hwnd);std::wcerr<<L"FAIL: EngineController creation.\n";return 7;}
    NovaVeyraSettings opts{};
    opts.size=sizeof(opts);
    opts.features=0; // First prove genuine decode + present.
    opts.fg_backend=NOVA_VEYRA_FG_VFG;
    opts.fg_multiplier=2;
    opts.sr_quality=2;
    if(!nova_veyra_open(engine,hwnd,argv[1],&opts)){
        nova_veyra_destroy(engine);DestroyWindow(hwnd);
        std::wcerr<<L"FAIL: EngineController rejected video open.\n";return 8;
    }
    NovaVeyraSnapshot snap{};
    snap.size=sizeof(snap);
    const auto deadline=GetTickCount64()+30000;
    bool decoded=false;
    bool requestedFg=false;
    bool observedFg=false;
    while(GetTickCount64()<deadline){
        pumpWindow();
        Sleep(100);
        snap={};snap.size=sizeof(snap);
        if(!nova_veyra_snapshot(engine,&snap)){
            std::wcerr<<L"FAIL: EngineController snapshot.\n";break;
        }
        if(snap.failed || snap.transport==NOVA_VEYRA_FAILED){
            std::wcerr<<L"FAIL: EngineController reported playback or D3D12 failure.\n";break;
        }
        decoded=decoded || (snap.source_frames>=10 && snap.running);
        if(decoded && argc==2){break;}
        if(decoded && argc==3 && !requestedFg){
            opts.features=NOVA_VEYRA_FEATURE_FG;
            requestedFg=true;
            if(!nova_veyra_settings(engine,&opts)){
                std::wcerr<<L"FAIL: VFG 2x request rejected by engine.\n";break;
            }
        }
        if(requestedFg && snap.fg_active && snap.generated_frames>0){
            observedFg=true;break;
        }
        if(snap.transport==NOVA_VEYRA_ENDED){break;}
    }
    printState(snap);
    nova_veyra_stop(engine);
    nova_veyra_destroy(engine);
    DestroyWindow(hwnd);
    if(!decoded){
        std::wcerr<<L"FAIL: no evidence of 10 actual decoded frames and running engine.\n";
        return 9;
    }
    if(argc==3 && !observedFg){
        std::wcerr<<L"FAIL: NVIDIA-VFG did not report active FG AND generated frames.\n";
        return 10;
    }
    std::wcout<<(argc==3?L"PASS: real Veyra decode + NVIDIA-VFG generated frames.\n":
                         L"PASS: real Veyra decoder and window produced frames.\n");
    return 0;
}
