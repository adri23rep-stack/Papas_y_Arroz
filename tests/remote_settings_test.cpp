#include "remote_settings.hpp"
#include <cassert>
#include <cstdio>
int main(){
    using namespace xs;
    assert(remote_setting_index("volume")==0);
    assert(remote_setting_index("unsupported")==-1);
    assert(valid_remote_setting(remote_setting_index("volume"),42));
    assert(!valid_remote_setting(remote_setting_index("volume"),101));
    assert(!valid_remote_setting(remote_setting_index("speed"),137));
    assert(valid_remote_setting(remote_setting_index("speed"),125));
    assert(!valid_remote_setting(-1,50));
    assert(remote_setting_index("motion_mode")==10);
    assert(valid_remote_setting(remote_setting_index("video_preset"),5));
    assert(!valid_remote_setting(remote_setting_index("video_preset"),6));
    assert(valid_remote_setting(remote_setting_index("motion_mode"),2));
    assert(!valid_remote_setting(remote_setting_index("motion_mode"),3));
    RemoteSettingsStore settings;
    assert(settings.get(0)==100);
    assert(settings.put(0,42,false));
    assert(settings.get(0)==42);
    assert(!settings.put(0,102,false));
    assert(settings.get(0)==42);
    return 0;
}
