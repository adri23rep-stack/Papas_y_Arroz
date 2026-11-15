#define SDL_MAIN_HANDLED
#include "remote_pairing_qr.hpp"
#include <cassert>
#include <string>
int main(){
    const auto payload=xs::pairing_qr_payload("192.168.1.2","012345");
    assert(payload=="xuperremote://pair?host=192.168.1.2&pin=012345");
    assert(xs::pairing_qr_payload("192.168.1.2","12345").empty());
    assert(xs::pairing_qr_payload("not-a-host","123456").empty());
    assert(xs::pairing_qr_payload("192.168.1.2&secret=x","123456").empty());
    QRcode* code=QRcode_encodeString(payload.c_str(),0,QR_ECLEVEL_M,QR_MODE_8,1);
    assert(code&&code->width>=21);
    QRcode_free(code);
    return 0;
}
