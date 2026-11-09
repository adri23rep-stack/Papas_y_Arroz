#pragma once
#include <mbedtls/version.h>
#include <mbedtls/md5.h>
// Mbed TLS 3 renamed *_ret one-shot hash functions; keep Switch 2.x support.
namespace xs {
inline int md5_digest(const unsigned char* input,size_t length,unsigned char output[16]){
#if MBEDTLS_VERSION_MAJOR >= 3
    return mbedtls_md5(input,length,output);
#else
    return mbedtls_md5_ret(input,length,output);
#endif
}
}
