// SPDX-License-Identifier: GPL-3.0-only
// Actual Veyra EngineController GPU smoke, no MPV/QML/fake frames.
// A DXGI Present call is NOT proof the frame reached physical scanout.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <chrono>
#include <cstdint>
#include <cwchar>
#include <iomanip>
#include <iostream>
#include "NovaVeyraApi.h"

namespace {
using Clock=std::chrono::steady_clock;
constexpr auto kPoll=std::chrono::milliseconds(100);
constexpr auto kTimeout=std::chrono::seconds(45);
constexpr auto kSustain=std::chrono::seconds(6);
constexpr auto kStall=std::chrono::milliseconds(1500);

LRESULT CALLBACK windowProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    if(msg==WM_DESTROY){PostQuitMessage(0);return 0;}
    return DefWindowProcW(hwnd,msg,wp,lp);
}
void pumpWindow(){
    MSG msg{};
    while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){
        TranslateMessage(&msg);DispatchMessageW(&msg);
    }
}
double asSeconds(Clock::duration d){return std::chrono::duration<double>(d).count();}
double asMilliseconds(Clock::duration d){return std::chrono::duration<double,std::milli>(d).count();}
void printStats(const NovaVeyraSnapshot& s,const NovaVeyraFlowStats& f){
    std::wcout<<std::fixed<<std::setprecision(2)
       <<L"transport="<<s.transport
       <<L" running="<<s.running
       <<L" failed="<<s.failed
       <<L" source_frames="<<s.source_frames
       <<L" generated_reported="<<s.generated_frames
       <<L" fps_source="<<s.source_fps
       <<L" fg_active="<<s.fg_active
       <<L" fg_backend="<<s.applied_fg_backend
       <<L" fg_multiplier="<<s.applied_fg_multiplier
       <<L" generated_ready="<<f.generated_ready
       <<L" real_submitted="<<f.real_submitted
       <<L" generated_submitted="<<f.generated_submitted
       <<L" generated_expired="<<f.generated_expired
       <<L" cancelled_before_present="<<f.cancelled_before_present
       <<L" fps_submit_window="<<f.present_submit_fps
       <<L" fps_generated_window="<<f.generated_submit_fps
       <<L" fps_low1_known="<<f.low1_known
       <<L" fps_low1_submit_window="<<f.present_low1_fps<<L'\n';
}
}

int wmain(int argc,wchar_t** argv){
    const bool testFg=argc==3&&wcscmp(argv[2],L"--fg2x")==0;
    if(argc<2||argc>3||(argc==3&&!testFg)){
        std::wcerr<<L"Usage: nova_veyra_native_smoke.exe <local-video.mp4> [--fg2x]\n";
        return 2;
    }
    const DWORD attrs=GetFileAttributesW(argv[1]);
    if(attrs==INVALID_FILE_ATTRIBUTES||(attrs&FILE_ATTRIBUTE_DIRECTORY)){
        std::wcerr<<L"FAIL: Missing readable local video.\n";return 3;
    }
    if(nova_veyra_abi()!=NOVA_VEYRA_NATIVE_ABI){
        std::wcerr<<L"FAIL: Incompatible native Veyra ABI.\n";return 4;
    }
    HINSTANCE inst=GetModuleHandleW(nullptr);
    WNDCLASSW wc{};
    wc.lpfnWndProc=windowProc;
    wc.hInstance=inst;
    wc.lpszClassName=L"NovaVeyraGPUAlphaSmoke";
    if(!RegisterClassW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS){
        std::wcerr<<L"FAIL: Cannot register video window.\n";return 5;
    }
    HWND hwnd=CreateWindowExW(0,wc.lpszClassName,L"Nova Veyra GPU Test",
              WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,960,540,
              nullptr,nullptr,inst,nullptr);
    if(!hwnd){std::wcerr<<L"FAIL: Cannot create video window.\n";return 6;}
    ShowWindow(hwnd,SW_SHOW);
    UpdateWindow(hwnd);

    void* engine=nova_veyra_create();
    if(!engine){DestroyWindow(hwnd);std::wcerr<<L"FAIL: Cannot create engine.\n";return 7;}
    NovaVeyraSettings options{};
    options.size=sizeof(options);
    options.features=0; // Decode first; then enable VFG.
    options.fg_backend=NOVA_VEYRA_FG_VFG;
    options.fg_multiplier=2;
    options.sr_quality=2;
    if(!nova_veyra_open(engine,hwnd,argv[1],&options)){
        nova_veyra_destroy(engine);DestroyWindow(hwnd);
        std::wcerr<<L"FAIL: Veyra rejected file open.\n";return 8;
    }

    NovaVeyraSnapshot state{};
    NovaVeyraFlowStats flow{};
    bool decoded=false,fgRequested=false,tracking=false,pass=false,failed=false;
    uint64_t firstSubmitted=0,firstGenerated=0,prevSubmitted=0;
    uint64_t measuredSubmitted=0,measuredGenerated=0;
    double averageSubmitFps=0,maxPresentGapMs=0;
    Clock::time_point trackStart{},lastPresent{};
    const auto deadline=Clock::now()+kTimeout;
    auto nextReport=Clock::now()+std::chrono::seconds(1);
    while(Clock::now()<deadline){
        pumpWindow();
        Sleep(DWORD(kPoll.count()));
        state={};state.size=sizeof(state);
        flow={};flow.size=sizeof(flow);
        if(!nova_veyra_snapshot(engine,&state)||
           !nova_veyra_flow_snapshot(engine,&flow)){
            std::wcerr<<L"FAIL: Engine or DXGI diagnostic snapshot failed.\n";
            failed=true;break;
        }
        if(state.failed||state.transport==NOVA_VEYRA_FAILED){
            std::wcerr<<L"FAIL: Veyra reported GPU/playback failure.\n";
            failed=true;break;
        }
        if(state.session_id!=flow.session_id){
            std::wcerr<<L"FAIL: Mismatched diagnostic session.\n";
            failed=true;break;
        }
        decoded=decoded||(state.running&&state.source_frames>=10);
        if(decoded&&!testFg){pass=true;break;}
        if(decoded&&testFg&&!fgRequested){
            // A 4s fixture cannot survive provider startup plus 6s sampling.
            if(state.duration_seconds>0&&state.duration_seconds<15.0){
                std::wcerr<<L"FAIL: MP4 too short for a sustained test. "
                          <<L"Use -Video with a local progressive MP4 >=20 seconds.\n";
                failed=true;break;
            }
            options.features=NOVA_VEYRA_FEATURE_FG;
            fgRequested=true;
            if(!nova_veyra_settings(engine,&options)){
                std::wcerr<<L"FAIL: VFG 2x request rejected.\n";
                failed=true;break;
            }
        }
        const auto now=Clock::now();
        if(fgRequested){
            const bool vfgOn=state.fg_active &&
                state.applied_fg_backend==NOVA_VEYRA_FG_VFG &&
                state.applied_fg_multiplier==2;
            const uint64_t submitted=flow.real_submitted+flow.generated_submitted;
            if(!tracking&&vfgOn&&flow.generated_submitted>0){
                tracking=true;
                trackStart=lastPresent=now;
                firstSubmitted=prevSubmitted=submitted;
                firstGenerated=flow.generated_submitted;
                std::wcout<<L"VFG generated DXGI Present detected. Measuring 6 seconds...\n";
            }else if(tracking){
                if(!vfgOn||flow.generated_submitted<firstGenerated||
                   submitted<prevSubmitted){
                    std::wcerr<<L"FAIL: VFG stopped or submission counters reset.\n";
                    failed=true;break;
                }
                if(submitted>prevSubmitted){
                    const double gap=asMilliseconds(now-lastPresent);
                    if(gap>maxPresentGapMs)maxPresentGapMs=gap;
                    lastPresent=now;
                    prevSubmitted=submitted;
                }else if(now-lastPresent>kStall){
                    std::wcerr<<L"FAIL: No new DXGI Present for 1500ms (stall).\n";
                    failed=true;break;
                }
                const double elapsed=asSeconds(now-trackStart);
                measuredSubmitted=submitted-firstSubmitted;
                measuredGenerated=flow.generated_submitted-firstGenerated;
                averageSubmitFps=elapsed>0?double(measuredSubmitted)/elapsed:0;
                if(now>=nextReport){
                    std::wcout<<std::fixed<<std::setprecision(2)
                         <<L"[VFG] seconds="<<elapsed
                         <<L" generated_submitted="<<measuredGenerated
                         <<L" output_submitted="<<measuredSubmitted
                         <<L" avg_submit_fps="<<averageSubmitFps
                         <<L" rolling_submit_fps="<<flow.present_submit_fps
                         <<L" max_gap_ms="<<maxPresentGapMs<<L'\n';
                    nextReport=now+std::chrono::seconds(1);
                }
                if(now-trackStart>=kSustain){
                    const double minFps=state.source_fps>0?1.5*state.source_fps:30.0;
                    pass=(measuredGenerated>=12&&averageSubmitFps>=minFps);
                    if(!pass){
                        std::wcerr<<L"FAIL: Insufficient 2x delivery; need 12+ "
                                  <<L"generated DXGI submits and >=1.5x source rate.\n";
                    }
                    break;
                }
            }
        }
        if(state.transport==NOVA_VEYRA_ENDED){
            std::wcerr<<L"FAIL: Video ended before 6 sustained seconds; "
                      <<L"use an MP4 >=20 seconds.\n";
            failed=true;break;
        }
    }

    printStats(state,flow);
    if(testFg){
        std::wcout<<std::fixed<<std::setprecision(2)
            <<L"sustained_generated_submitted="<<measuredGenerated
            <<L" sustained_output_submitted="<<measuredSubmitted
            <<L" sustained_avg_submit_fps="<<averageSubmitFps
            <<L" longest_present_gap_ms="<<maxPresentGapMs<<L'\n'
            <<L"NOTE: Present submission is NOT measured monitor scanout. "
            <<L"Use PresentMon for screen-level verification.\n";
    }
    nova_veyra_stop(engine);
    nova_veyra_destroy(engine);
    DestroyWindow(hwnd);
    if(!decoded){std::wcerr<<L"FAIL: No evidence of 10 decoded frames.\n";return 9;}
    if(failed||!pass){
        std::wcerr<<L"FAIL: Strict Veyra hardware test did not pass.\n";
        return 10;
    }
    std::wcout<<(testFg?
        L"PASS: 6 seconds of sustained VFG-generated DXGI Present calls.\n":
        L"PASS: Real Veyra decoder and window produced frames.\n");
    return 0;
}
