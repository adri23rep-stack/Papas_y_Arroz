#include "nova_native_source_policy.hpp"
#include <cassert>
#include <initializer_list>
int main(){
    using xs::NovaNativeSourcePolicy;
    assert(xs::nativeHttpsQuery(true,false,true,true,false));
    assert(!xs::nativeHttpsQuery(true,true,true,true,false));
    assert(!xs::nativeHttpsQuery(true,false,false,true,false));
    assert(!xs::nativeHttpsQuery(true,false,true,false,false));
    assert(!xs::nativeHttpsQuery(true,false,true,true,true));
    assert(!xs::nativeHttpsQuery(false,false,true,true,false));
    for(bool local:{false,true})for(bool http:{false,true})
    for(bool live:{false,true})for(bool beta:{false,true})
    for(bool headers:{false,true})for(bool subtitles:{false,true}){
        NovaNativeSourcePolicy p{local,http,live,beta,headers,subtitles};
        assert(p.eligible()==((local||http)&&(!live||beta)&&!headers&&!subtitles));
        if(headers||subtitles||(!local&&!http)||(live&&!beta)){
            assert(!p.eligible());assert(!p.strict(true));
        }
        assert(!p.strict(false));
        if(live||!local)assert(!p.strict(true));
    }
    assert((NovaNativeSourcePolicy{true,false,false,false,false,false}.strict(true)));
    assert((NovaNativeSourcePolicy{false,true,true,true,false,false}.eligible()));
}
