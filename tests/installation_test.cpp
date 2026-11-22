#include "installation.hpp"
#include <filesystem>
#include <cassert>
#include <chrono>
#include <iostream>
#include <algorithm>
#include <cctype>

int main(){
    const auto root=std::filesystem::temp_directory_path()/("xs-install-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(root);
    xs::InstallationJson base={{"portal_hosts",{"https://example.invalid"}},{"key_base64","synthetic"},{"password_salt","synthetic"},{"app_id","test.app"}};
    int calls=0;
    auto entropy=[&](unsigned char* p,size_t n){++calls;for(size_t i=0;i<n;++i)p[i]=static_cast<unsigned char>(calls+i);return true;};
    xs::InstallationJson a,b,c;std::string error;
    for(const auto* name:{"one","two","derived","state","alt-state","invalid"}){
        std::filesystem::create_directory(root/name);
        std::ofstream(root/name/"service.template.json")<<base.dump();
    }
    const auto one=(root/"one/service.json").string();
    const auto two=(root/"two/service.json").string();
    const auto derived=(root/"derived/service.json").string();

    // Legacy behavior is unchanged and still persists after opt-in is set.
    assert(xs::load_installation(one,a,error,entropy));
    assert(a["installation_serial"].get<std::string>().size()==32);
    assert(a["device_id"].get<std::string>().size()==17);
    assert(!a.contains("identity_scheme")&&!a.contains("android_id"));
    const auto legacy=a;
    assert(xs::load_installation(one,b,error,entropy,true)&&a==b&&calls==1);
    assert(xs::load_installation(two,b,error,entropy));
    assert(a["installation_serial"]!=b["installation_serial"]&&a["device_id"]!=b["device_id"]);

    // Lab mode: serial, MAC and Android ID come from a single installation
    // seed, never from the APK, an existing emulator, or a server token.
    assert(xs::load_installation(derived,c,error,entropy,true));
    assert(c["identity_scheme"]=="derived-v1");
    const auto serial=c["installation_serial"].get<std::string>();
    const auto mac=c["device_id"].get<std::string>();
    const auto android=c["android_id"].get<std::string>();
    assert(serial.size()==32&&mac.size()==17&&android.size()==16);
    const auto all_hex=[](const std::string& value){
        return std::all_of(value.begin(),value.end(),[](unsigned char x){return std::isxdigit(x)!=0;});
    };
    assert(all_hex(serial)&&all_hex(android));
    unsigned char mac_hash[32]{},android_hash[32]{};
    assert(xs::installation_sha256("nova:mac:v1:"+serial,mac_hash));
    assert(xs::installation_sha256("nova:android-id:v1:"+serial,android_hash));
    mac_hash[0]=static_cast<unsigned char>((mac_hash[0]&0xfe)|0x02);
    std::string expected_mac;
    for(size_t i=0;i<6;++i){if(i)expected_mac+=':';expected_mac+=xs::installation_hex(mac_hash+i,1);}
    assert(mac==expected_mac);
    assert(android==xs::installation_hex(android_hash,8));
    assert((mac_hash[0]&0x03)==0x02); // locally administered and unicast
    const auto stable=c;
    assert(xs::load_installation(derived,c,error,entropy,false)&&stable==c&&calls==3);
    assert(std::filesystem::exists(derived));
    assert(legacy==a);

    // Losing a config must not silently fabricate an identity on top of an
    // existing activation, even when a test profile is in use.
    std::ofstream(root/"state/device_state.json")<<"{}";
    assert(!xs::load_installation((root/"state/service.json").string(),b,error,entropy,true)&&calls==3);
    std::ofstream(root/"alt-state/device_state.android-tv-test.json")<<"{}";
    assert(!xs::load_installation((root/"alt-state/service.json").string(),b,error,entropy,true)&&calls==3);
    std::ofstream(root/"invalid/service.json")<<"broken";
    assert(!xs::load_installation((root/"invalid/service.json").string(),b,error,entropy,true)&&calls==3);
    base["installation_serial"]="private";
    std::ofstream(root/"two/service.template.json")<<base.dump();
    std::filesystem::remove(two);
    assert(!xs::load_installation(two,b,error,entropy,true)&&calls==3);

    std::filesystem::remove_all(root);
    std::cout<<"PASS: legacy identity and persistence; deterministic derived identifiers; activation protection; no network.\n";
}
