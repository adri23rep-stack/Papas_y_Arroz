// SPDX-License-Identifier: GPL-3.0-only
extern "C" {
#include <libavutil/log.h>
}
int quietNetworkDecoder(){
    const int previous=av_log_get_level();
    av_log_set_level(AV_LOG_QUIET);
    return previous;
}
void restoreNetworkDecoderLog(int previous){av_log_set_level(previous);}
