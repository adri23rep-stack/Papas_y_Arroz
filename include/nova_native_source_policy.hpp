#pragma once
namespace xs {
inline bool nativeHttpsQuery(bool query,bool live,bool https,bool hasHost,bool hasUserInfo){
    return query && !live && https && hasHost && !hasUserInfo;
}
struct NovaNativeSourcePolicy {
    bool local=false, safe_http=false, live=false, public_live_beta=false;
    bool has_headers=false, has_subtitles=false;
    bool eligible() const {
        return (local||safe_http)&&(!live||public_live_beta)&&!has_headers&&!has_subtitles;
    }
    // Strict mode applies only to a source supported by the native pilot.
    // Protected provider content must retain the MPV path even in the Alpha.
    bool strict(bool requested) const {return requested&&local&&!live&&eligible();}
};
}
