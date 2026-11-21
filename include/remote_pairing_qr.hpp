#pragma once
#if defined(_WIN32) || defined(__APPLE__)
#include <qrencode.h>
#include <SDL.h>
#include <algorithm>
#include <cctype>
#include <string>
namespace xs {
inline std::string pairing_qr_payload(const std::string& address,
                                      const std::string& pin){
    if(address.empty()||address.size()>15||pin.size()!=6||
       !std::all_of(pin.begin(),pin.end(),[](unsigned char c){return std::isdigit(c)!=0;})||
       !std::all_of(address.begin(),address.end(),
          [](unsigned char c){return std::isdigit(c)||c=='.';}))return {};
    return "xuperremote://pair?host="+address+"&pin="+pin;
}
// Returns false instead of drawing a fake or invalid QR code.
inline bool render_pairing_qr(SDL_Renderer* renderer,const std::string& payload,
                              int x,int y,int available){
    if(!renderer||payload.empty())return false;
    QRcode* qr=QRcode_encodeString(payload.c_str(),0,QR_ECLEVEL_M,QR_MODE_8,1);
    if(!qr)return false;
    const int quiet=4;
    const int modules=qr->width+quiet*2;
    const int cell=available/modules;
    if(cell<2){QRcode_free(qr);return false;}
    const int side=modules*cell;
    SDL_SetRenderDrawColor(renderer,255,255,255,255);
    SDL_Rect backdrop{x,y,side,side};SDL_RenderFillRect(renderer,&backdrop);
    SDL_SetRenderDrawColor(renderer,14,19,33,255);
    for(int row=0;row<qr->width;++row)
        for(int col=0;col<qr->width;++col)
            if(qr->data[row*qr->width+col]&1){
                SDL_Rect square{x+(col+quiet)*cell,y+(row+quiet)*cell,cell,cell};
                SDL_RenderFillRect(renderer,&square);
            }
    QRcode_free(qr);
    return true;
}
}
#endif
