#include "youtube_remote.hpp"
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#include <algorithm>
#include <cctype>
#include <cwctype>
#include <string>

namespace xs {
std::string youtube_results_url(const std::string& query){
    if(query.size()<2||query.size()>120)return "";
    constexpr char digits[]="0123456789ABCDEF";
    std::string url="https://www.youtube.com/results?search_query=";
    bool any=false;
    for(unsigned char c:query){
        if(c<32||c==127)return "";
        if(std::isalnum(c)&&c<128){
            url+=static_cast<char>(c);
            any=true;
        }else if(c=='-'||c=='_'||c=='.'||c=='~'){
            url+=static_cast<char>(c);any=true;
        }else if(c==' '){
            url+="%20";
        }else{
            url+='%';url+=digits[c>>4];url+=digits[c&15];
            any=true;
        }
    }
    return any?url:"";
}
static bool browser_open(const std::string& address){
    if(address.rfind("https://www.youtube.com/",0)!=0)return false;
    const std::wstring w(address.begin(),address.end()); // percent-encoded ASCII URL
    const HINSTANCE result=ShellExecuteW(nullptr,L"open",w.c_str(),nullptr,nullptr,SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result)>32;
}
bool youtube_launch(){
    return browser_open("https://www.youtube.com/");
}
bool youtube_search(const std::string& query){
    const std::string address=youtube_results_url(query);
    return !address.empty()&&browser_open(address);
}
bool youtube_foreground(){
    const HWND window=GetForegroundWindow();
    if(!window)return false;
    wchar_t text[1024]{};
    if(!GetWindowTextW(window,text,1024))return false;
    std::wstring title(text);
    std::transform(title.begin(),title.end(),title.begin(),
                   [](wchar_t c){return static_cast<wchar_t>(std::towlower(c));});
    return title.find(L"youtube")!=std::wstring::npos;
}
static bool key(WORD vk,WORD modifier=0){
    INPUT keys[4]{};
    UINT count=0;
    auto add=[&](WORD code,DWORD flags){
        INPUT input{};input.type=INPUT_KEYBOARD;
        input.ki.wVk=code;input.ki.dwFlags=flags;
        keys[count++]=input;
    };
    if(modifier)add(modifier,0);
    add(vk,0);add(vk,KEYEVENTF_KEYUP);
    if(modifier)add(modifier,KEYEVENTF_KEYUP);
    return SendInput(count,keys,sizeof(INPUT))==count;
}
static bool mouse(DWORD flags){
    INPUT input{};input.type=INPUT_MOUSE;input.mi.dwFlags=flags;
    return SendInput(1,&input,sizeof(INPUT))==1;
}
bool youtube_input(const std::string& command,int dx,int dy){
    // Global Windows input is only sent while the foreground window's title
    // explicitly identifies YouTube. No arbitrary OS keyboard or URLs.
    // Always release an earlier mouse drag, even if Windows unexpectedly
    // changes focus; otherwise the left button could remain held.
    if(command=="pointer_up")return mouse(MOUSEEVENTF_LEFTUP);
    if(!youtube_foreground())return false;
    if(command=="pointer_move"){
        POINT p{};
        if(!GetCursorPos(&p))return false;
        const int left=GetSystemMetrics(SM_XVIRTUALSCREEN);
        const int top=GetSystemMetrics(SM_YVIRTUALSCREEN);
        const int right=left+GetSystemMetrics(SM_CXVIRTUALSCREEN)-1;
        const int bottom=top+GetSystemMetrics(SM_CYVIRTUALSCREEN)-1;
        return SetCursorPos(std::clamp(p.x+dx,left,right),
                            std::clamp(p.y+dy,top,bottom))!=0;
    }
    if(command=="pointer_down")return mouse(MOUSEEVENTF_LEFTDOWN);
    if(command=="click")return mouse(MOUSEEVENTF_LEFTDOWN)&&mouse(MOUSEEVENTF_LEFTUP);
    if(command=="up")return key(VK_UP);
    if(command=="down")return key(VK_DOWN);
    if(command=="left")return key(VK_LEFT);
    if(command=="right")return key(VK_RIGHT);
    if(command=="ok")return key(VK_RETURN);
    if(command=="youtube_pause"||command=="play_pause")return key('K');
    if(command=="youtube_fullscreen")return key('F');
    if(command=="youtube_mute")return key('M');
    if(command=="youtube_seek_back")return key('J');
    if(command=="youtube_seek_forward")return key('L');
    if(command=="youtube_next")return key('N',VK_SHIFT);
    if(command=="youtube_previous")return key('P',VK_SHIFT);
    if(command=="youtube_back")return key(VK_LEFT,VK_MENU);
    if(command=="youtube_volume_up")return key(VK_VOLUME_UP);
    if(command=="youtube_volume_down")return key(VK_VOLUME_DOWN);
    return false;
}
}
#endif
