#pragma once
#define JSON_NOEXCEPTION 1
#include "json.hpp"
#include <fstream>
#include <cstdio>
#include <cerrno>
#include <functional>
#include <string>
#include <mbedtls/sha256.h>
#include <mbedtls/version.h>
#include <sys/stat.h>
namespace xs {
using InstallationJson=nlohmann::json;
// Local identity experiment only: one random installation seed, two
// domain-separated SHA-256 derivatives. Never reuse another app's tokens.
inline bool installation_sha256(const std::string& input,unsigned char out[32]){
#if MBEDTLS_VERSION_MAJOR >= 3
    return mbedtls_sha256(reinterpret_cast<const unsigned char*>(input.data()),input.size(),out,0)==0;
#else
    return mbedtls_sha256_ret(reinterpret_cast<const unsigned char*>(input.data()),input.size(),out,0)==0;
#endif
}
inline std::string installation_hex(const unsigned char* data,size_t size){
    constexpr char hex[]="0123456789abcdef";
    std::string result;result.reserve(size*2);
    for(size_t i=0;i<size;++i){result+=hex[data[i]>>4];result+=hex[data[i]&15];}
    return result;
}

inline bool read_installation_json(const std::string& path,InstallationJson& out){
    std::ifstream f(path,std::ios::binary|std::ios::ate);const auto n=f.tellg();if(n<0||n>65536)return false;
    f.seekg(0);std::string data(static_cast<size_t>(n),'\0');f.read(data.data(),n);if(!f)return false;
    out=InstallationJson::parse(data,nullptr,false);return out.is_object();
}
// A common template has no installation/account identity. Existing installations
// always keep their configuration; a missing config with activation state is not
// replaced by a new identity. Generation happens only before the first API call.
inline bool load_installation(const std::string& path,InstallationJson& config,std::string& error,
                              const std::function<bool(unsigned char*,size_t)>& entropy,
                              bool experimental_derived_identity=false){
    struct stat st{};
    if(stat(path.c_str(),&st)==0){if(read_installation_json(path,config))return true;error="Configuracion existente invalida; restaura service.json.";return false;}
    if(errno!=ENOENT){error="No se pudo leer la configuracion de esta instalacion.";return false;}
    const auto slash=path.find_last_of("/\\");const std::string folder=slash==std::string::npos?"":path.substr(0,slash+1);
    for(const char* state:{"device_state.json","device_state.android-test.json","device_state.android-tv-test.json"}){
        if(stat((folder+state).c_str(),&st)==0){error="Conserva la activacion: restaura tu service.json anterior.";return false;}
        if(errno!=ENOENT){error="No se pudo comprobar el estado de activacion.";return false;}
    }
    if(!read_installation_json(folder+"service.template.json",config)){error="Falta service.template.json junto al NRO.";return false;}
    for(const char* key:{"installation_serial","device_id","android_id","identity_scheme","userId","userToken","sn","password","username"})if(config.contains(key)){
        error="La plantilla publica contiene datos de instalacion; usa el paquete oficial.";return false;
    }
    auto nonempty=[&](const char* key){auto i=config.find(key);return i!=config.end()&&i->is_string()&&!i->get_ref<const std::string&>().empty();};
    if(!config.contains("portal_hosts")||!config["portal_hosts"].is_array()||config["portal_hosts"].empty()||!nonempty("key_base64")||!nonempty("password_salt")||!nonempty("app_id")){
        error="Plantilla de conexion incompleta.";return false;
    }
    unsigned char random[22]{};if(!entropy(random,sizeof(random))){error="No se pudo crear la identidad de esta instalacion.";return false;}
    const std::string serial=installation_hex(random,16);
    std::string device;
    if(experimental_derived_identity){
        // Derive values from this installation's own seed; do NOT copy Focuz
        // identities, fingerprint, credentials, or trial-reprovision behavior.
        unsigned char mac_digest[32]{},id_digest[32]{};
        if(!installation_sha256("nova:mac:v1:"+serial,mac_digest)||
           !installation_sha256("nova:android-id:v1:"+serial,id_digest)){
            error="No se pudo preparar la identidad local.";return false;
        }
        mac_digest[0]=static_cast<unsigned char>((mac_digest[0]&0xfe)|0x02);
        for(size_t i=0;i<6;++i){if(i)device+=':';device+=installation_hex(mac_digest+i,1);}
        config["android_id"]=installation_hex(id_digest,8);
        config["identity_scheme"]="derived-v1";
    }else{
        random[16]=static_cast<unsigned char>((random[16]&0xfe)|0x02);
        for(size_t i=16;i<22;++i){if(i>16)device+=':';device+=installation_hex(random+i,1);}
    }
    config["installation_serial"]=serial;config["device_id"]=device;
    const auto temp=path+".tmp";std::ofstream f(temp,std::ios::binary|std::ios::trunc);const auto data=config.dump(2)+"\n";f.write(data.data(),data.size());f.close();
    if(!f.good()||std::rename(temp.c_str(),path.c_str())!=0){error="No se pudo guardar service.json en la SD.";return false;}
    return true;
}
}
