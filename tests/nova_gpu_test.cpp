#include "nova_gpu.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

int main(){
    namespace fs=std::filesystem;
    const auto root=fs::temp_directory_path()/"nova_gpu_test";
    fs::remove_all(root);
    assert(!xs::nova_rife_files_present(root));
    const auto gpu=root/"nova-gpu";
    const auto model=gpu/"models"/"rife-v4.6_ensembleFalse";
    fs::create_directories(model);
    for(const auto& f:{
        root/"libvapoursynth.dll",root/"libvapoursynth-script-0.dll",
        gpu/"rife.vpy",gpu/"librife_windows_x86-64.dll",
        model/"flownet.bin",model/"flownet.param"
    }) {
        std::ofstream out(f,std::ios::binary);out<<"test";
    }
    assert(xs::nova_rife_files_present(root));
    fs::remove(gpu/"librife_windows_x86-64.dll");
    assert(!xs::nova_rife_files_present(root));
    fs::remove_all(root);
    return 0;
}
