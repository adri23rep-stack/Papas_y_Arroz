#include "native_engine.hpp"
#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc,char** argv){
    if(argc==2&&std::string(argv[1])=="--self-test"){
        const xs::NativeProbeResult bad=xs::native_probe_local_video("",24);
        if(bad.ok||bad.diagnostic.empty())return 2;
        std::cout<<"Native decoder API ready | FFmpeg | D3D11VA fallback: supported\n";
        return 0;
    }
    if(argc<2||argc>4){
        std::cerr<<"XuperNativeProbe <local-video-path> [max-frames] [cpu]\n";
        return 2;
    }
    int frames=24;
    if(argc>=3){
        try{frames=std::stoi(argv[2]);}catch(...){return 2;}
    }
    const bool prefer_gpu=argc<4||std::string(argv[3])!="cpu";
    auto result=xs::native_probe_local_video(argv[1],frames,prefer_gpu);
    std::cout<<result.diagnostic<<"\n";
    std::cout<<"codec="<<result.codec<<" resolution="<<result.width<<"x"<<result.height
             <<" fps="<<result.nominal_fps<<" video_frames="<<result.video_frames
             <<" audio_tracks="<<result.audio_streams<<"\n";
    std::cout<<"hw_requested="<<(result.hardware_requested?"yes":"no")
             <<" hw_available="<<(result.hardware_available?"yes":"no")
             <<" decoder="<<(result.active_decoder==xs::NativeDecoderKind::D3D11VA?"d3d11va":"cpu")
             <<"\n";
    return result.ok?0:1;
}
