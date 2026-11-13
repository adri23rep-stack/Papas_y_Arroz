#include "native_engine.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/hwcontext.h>
}
#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>

namespace xs {
namespace {
struct FormatCloser {void operator()(AVFormatContext* p) const {if(p)avformat_close_input(&p);}};
struct DecoderCloser {void operator()(AVCodecContext* p) const {if(p)avcodec_free_context(&p);}};
struct PacketCloser {void operator()(AVPacket* p) const {if(p)av_packet_free(&p);}};
struct FrameCloser {void operator()(AVFrame* p) const {if(p)av_frame_free(&p);}};
struct BufferCloser {void operator()(AVBufferRef* p) const {if(p)av_buffer_unref(&p);}};
using Format=std::unique_ptr<AVFormatContext,FormatCloser>;
using Decoder=std::unique_ptr<AVCodecContext,DecoderCloser>;
using Packet=std::unique_ptr<AVPacket,PacketCloser>;
using Frame=std::unique_ptr<AVFrame,FrameCloser>;
using Buffer=std::unique_ptr<AVBufferRef,BufferCloser>;

std::string av_error(int code){
    char buf[AV_ERROR_MAX_STRING_SIZE]={};
    av_strerror(code,buf,sizeof(buf));
    return buf;
}
AVPixelFormat choose_pixel_format(AVCodecContext* ctx,const AVPixelFormat* offered){
    // The context's own D3D11 hw device must be initialized to request it.
    if(ctx->hw_device_ctx){
        for(const AVPixelFormat* p=offered;*p!=AV_PIX_FMT_NONE;++p)
            if(*p==AV_PIX_FMT_D3D11)return *p;
    }
    // Skip hardware formats on the CPU fallback path.
    for(const AVPixelFormat* p=offered;*p!=AV_PIX_FMT_NONE;++p){
        const AVPixFmtDescriptor* d=av_pix_fmt_desc_get(*p);
        if(d&&!(d->flags&AV_PIX_FMT_FLAG_HWACCEL))return *p;
    }
    return offered[0];
}
bool codec_supports_d3d11(const AVCodec* codec){
    for(int i=0;;++i){
        const AVCodecHWConfig* config=avcodec_get_hw_config(codec,i);
        if(!config)return false;
        if(config->device_type==AV_HWDEVICE_TYPE_D3D11VA
           &&(config->methods&AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX))
            return true;
    }
}
} // namespace

NativeProbeResult native_probe_local_video(const std::string& path,int max_frames,bool prefer_hardware){
    NativeProbeResult out;
    out.hardware_requested=prefer_hardware;
    if(path.empty()||max_frames<1||max_frames>900){
        out.diagnostic="Ruta o limite de fotogramas invalido.";
        return out;
    }
    // No arbitrary URLs, redirects, cookie files or network streams in the
    // new engine probe. This protects the existing authenticated source flow.
    std::error_code fs_error;
    if(!std::filesystem::is_regular_file(path,fs_error)){
        out.diagnostic="El archivo local no existe.";
        return out;
    }

    AVFormatContext* raw_format=nullptr;
    const int opened=avformat_open_input(&raw_format,path.c_str(),nullptr,nullptr);
    Format format(raw_format);
    if(opened<0||!format){
        out.diagnostic="FFmpeg no pudo abrir el video: "+av_error(opened);
        return out;
    }
    int rc=avformat_find_stream_info(format.get(),nullptr);
    if(rc<0){out.diagnostic="FFmpeg no pudo leer pistas: "+av_error(rc);return out;}
    int video=av_find_best_stream(format.get(),AVMEDIA_TYPE_VIDEO,-1,-1,nullptr,0);
    if(video<0){out.diagnostic="El archivo no contiene una pista de video.";return out;}
    for(unsigned int i=0;i<format->nb_streams;++i)
        if(format->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO)++out.audio_streams;
    const AVStream* stream=format->streams[video];
    const AVCodec* codec=avcodec_find_decoder(stream->codecpar->codec_id);
    if(!codec){out.diagnostic="No hay decodificador para el video.";return out;}
    out.codec=codec->name?codec->name:"unknown";
    out.width=stream->codecpar->width;out.height=stream->codecpar->height;
    if(stream->avg_frame_rate.den>0)
        out.nominal_fps=av_q2d(stream->avg_frame_rate);

    const bool can_hw=prefer_hardware&&codec_supports_d3d11(codec);
    Buffer device;
    if(can_hw){
        AVBufferRef* raw_device=nullptr;
        const int device_rc=av_hwdevice_ctx_create(&raw_device,AV_HWDEVICE_TYPE_D3D11VA,nullptr,nullptr,0);
        if(device_rc>=0&&raw_device){device.reset(raw_device);out.hardware_available=true;}
        else if(raw_device)av_buffer_unref(&raw_device);
    }
    auto new_decoder=[&](bool use_gpu)->Decoder {
        Decoder decoder(avcodec_alloc_context3(codec));
        if(!decoder)return {};
        if(avcodec_parameters_to_context(decoder.get(),stream->codecpar)<0)return {};
        decoder->get_format=choose_pixel_format;
        if(use_gpu&&device){
            decoder->hw_device_ctx=av_buffer_ref(device.get());
            if(!decoder->hw_device_ctx)return {};
        }
        if(avcodec_open2(decoder.get(),codec,nullptr)<0)return {};
        return decoder;
    };
    Decoder decoder=new_decoder(static_cast<bool>(device));
    if(!decoder){
        // GPU failures are not fatal: retry a new clean decoder on CPU.
        out.hardware_available=false;
        decoder=new_decoder(false);
    }
    if(!decoder){out.diagnostic="No se pudo iniciar el decodificador, ni por CPU.";return out;}
    Packet packet(av_packet_alloc());
    Frame frame(av_frame_alloc());
    if(!packet||!frame){out.diagnostic="No hay memoria para los fotogramas.";return out;}

    auto drain=[&](){
        while(out.video_frames<max_frames){
            const int state=avcodec_receive_frame(decoder.get(),frame.get());
            if(state==AVERROR(EAGAIN)||state==AVERROR_EOF)return true;
            if(state<0){out.diagnostic="Fallo al decodificar: "+av_error(state);return false;}
            ++out.video_frames;
            if(frame->format==AV_PIX_FMT_D3D11)out.active_decoder=NativeDecoderKind::D3D11VA;
            av_frame_unref(frame.get());
        }
        return true;
    };
    bool valid=true;
    int scanned=0;
    while(out.video_frames<max_frames&&++scanned<=25000){
        const int state=av_read_frame(format.get(),packet.get());
        if(state==AVERROR_EOF)break;
        if(state<0){out.diagnostic="Fallo al leer paquetes: "+av_error(state);valid=false;break;}
        if(packet->stream_index==video){
            const int sent=avcodec_send_packet(decoder.get(),packet.get());
            if(sent<0&&sent!=AVERROR(EAGAIN)){
                out.diagnostic="Fallo al enviar paquete: "+av_error(sent);valid=false;
            }
            if(valid)valid=drain();
        }
        av_packet_unref(packet.get());
        if(!valid)break;
    }
    if(valid&&out.video_frames<max_frames){
        const int flushed=avcodec_send_packet(decoder.get(),nullptr);
        if(flushed>=0||flushed==AVERROR_EOF)valid=drain();
    }
    out.ok=valid&&out.video_frames>0;
    if(!out.ok&&out.diagnostic.empty())out.diagnostic="Ningun fotograma pudo decodificarse.";
    if(out.ok)
        out.diagnostic=std::string("Decodificacion nativa: ")+
            (out.active_decoder==NativeDecoderKind::D3D11VA?"D3D11VA GPU":"CPU")+
            " | "+std::to_string(out.video_frames)+" fotogramas";
    return out;
}
}
