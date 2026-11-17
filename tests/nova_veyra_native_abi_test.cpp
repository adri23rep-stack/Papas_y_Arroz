// Verify stable cross-compiler contract independently of NVIDIA SDK/runtime.
// Do not use false positives from Veyra's real GPU path in CI.
#include "NovaVeyraApi.h"
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <cassert>

static_assert(NOVA_VEYRA_NATIVE_ABI==1u);
static_assert(std::is_standard_layout<NovaVeyraSettings>::value);
static_assert(std::is_trivial<NovaVeyraSettings>::value);
static_assert(std::is_standard_layout<NovaVeyraSnapshot>::value);
static_assert(std::is_trivial<NovaVeyraSnapshot>::value);
static_assert(offsetof(NovaVeyraSettings,size)==0);
static_assert(offsetof(NovaVeyraSnapshot,size)==0);
static_assert(sizeof(NovaVeyraSettings)==24);
static_assert(sizeof(NovaVeyraSnapshot)==104);
static_assert(NOVA_VEYRA_FG_DLSS==0 && NOVA_VEYRA_FG_VFG==4);
static_assert(NOVA_VEYRA_EMPTY==0 && NOVA_VEYRA_ENDED==4 && NOVA_VEYRA_FAILED==6);
using OpenFn=int (NOVA_VEYRA_CALL*)(void*,void*,const wchar_t*,const NovaVeyraSettings*);
using SnapshotFn=int (NOVA_VEYRA_CALL*)(void*,NovaVeyraSnapshot*);
static_assert(std::is_same<decltype(&nova_veyra_open),OpenFn>::value);
static_assert(std::is_same<decltype(&nova_veyra_snapshot),SnapshotFn>::value);
int main(){
    NovaVeyraSettings settings{};
    settings.size=sizeof(settings);
    settings.features=NOVA_VEYRA_FEATURE_SR|NOVA_VEYRA_FEATURE_FG;
    settings.fg_backend=NOVA_VEYRA_FG_DLSS;
    settings.fg_multiplier=2;
    settings.sr_quality=2;
    NovaVeyraSnapshot snapshot{};
    snapshot.size=sizeof(snapshot);
    assert(snapshot.size==104);
    assert((settings.features & NOVA_VEYRA_FEATURE_FG)!=0);
    assert((settings.features & NOVA_VEYRA_FEATURE_NR)==0);
    assert((NOVA_VEYRA_FEATURE_NR|NOVA_VEYRA_FEATURE_SR|NOVA_VEYRA_FEATURE_FG|NOVA_VEYRA_FEATURE_HDR)==15);
    assert(settings.size==24);
    return 0;
}
