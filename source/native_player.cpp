// XuperTV Native Player v2: independent local-media video+audio player.
// This is deliberately separate from the existing authenticated XuperTV UI.
// FFmpeg demux/decoding + D3D11VA when available, SDL D3D11 presentation,
// stereo audio queue, A/V clock, seeking, pause and a basic information HUD.
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_ttf.h>
#include "native_playback_utils.hpp"
#include "native_subtitles.hpp"
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/hwcontext.h>
#include <libavutil/pixdesc.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
std::string av_err(int rc){
    char b[AV_ERROR_MAX_STRING_SIZE]{};
    av_strerror(rc,b,sizeof(b));
    return b;
}
AVPixelFormat select_hw_or_cpu(AVCodecContext* ctx,const AVPixelFormat* formats){
    if(ctx->hw_device_ctx){
        for(auto p=formats;*p!=AV_PIX_FMT_NONE;++p)
            if(*p==AV_PIX_FMT_D3D11)return *p;
    }
    for(auto p=formats;*p!=AV_PIX_FMT_NONE;++p){
        const auto* desc=av_pix_fmt_desc_get(*p);
        if(desc&&!(desc->flags&AV_PIX_FMT_FLAG_HWACCEL))return *p;
    }
    return formats[0];
}
bool supports_d3d11(const AVCodec* codec){
    for(int i=0;;++i){
        const auto* cfg=avcodec_get_hw_config(codec,i);
        if(!cfg)return false;
        if(cfg->device_type==AV_HWDEVICE_TYPE_D3D11VA&&
           (cfg->methods&AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX))return true;
    }
}
struct NativePlayer {
    AVFormatContext* fmt=nullptr;
    AVCodecContext* vdec=nullptr;
    AVCodecContext* adec=nullptr;
    AVBufferRef* hw_device=nullptr;
    SwsContext* sws=nullptr;
    SwrContext* swr=nullptr;
    AVPacket* packet=nullptr;
    AVFrame* frame=nullptr;
    AVFrame* cpu_frame=nullptr;
    SDL_Window* window=nullptr;
    SDL_Renderer* renderer=nullptr;
    SDL_Texture* texture=nullptr;
    SDL_AudioDeviceID audio_device=0;
    TTF_Font* font=nullptr;
    int video_index=-1,audio_index=-1,src_w=0,src_h=0;
    int sample_rate=48000,volume=100,rendered=0,dropped=0,requested_frames=0,audio_samples_decoded=0;
    int audio_input_rate=0,audio_input_fmt=-1,audio_input_channels=0;
    bool running=true,paused=false,fullscreen=false,verbose=true,nearest=false;
    bool test_mode=false,smoke_audio=false,hw_active=false,has_audio_pts=false,clock_started=false;
    bool did_sdl=false,did_ttf=false,valid=false,seeking=false;
    double duration=0,last_video_pts=0,audio_end_pts=0,clock_origin=0,seek_to=-1;
    Uint64 tick_origin=0,tick_pause_started=0;
    std::string decoder_name="CPU",render_name="uninitialized",error,source,codec_name;
    std::vector<uint8_t> pixels;
    std::string screenshot_path;
    bool screenshot_ok=false;
    std::vector<int> audio_tracks;
    std::vector<xs::NativeSubtitleCue> sidecar_subtitles;
    bool subtitle_visible=true;
    ~NativePlayer(){
        if(audio_device)SDL_CloseAudioDevice(audio_device);
        if(font)TTF_CloseFont(font);
        if(did_ttf)TTF_Quit();
        if(texture)SDL_DestroyTexture(texture);
        if(renderer)SDL_DestroyRenderer(renderer);
        if(window)SDL_DestroyWindow(window);
        if(did_sdl)SDL_Quit();
        if(sws)sws_freeContext(sws);
        if(swr)swr_free(&swr);
        if(frame)av_frame_free(&frame);
        if(cpu_frame)av_frame_free(&cpu_frame);
        if(packet)av_packet_free(&packet);
        if(vdec)avcodec_free_context(&vdec);
        if(adec)avcodec_free_context(&adec);
        if(hw_device)av_buffer_unref(&hw_device);
        if(fmt)avformat_close_input(&fmt);
    }
    double frame_pts(const AVFrame* f,int stream) const {
        if(!f||f->best_effort_timestamp==AV_NOPTS_VALUE||stream<0)
            return stream==video_index?last_video_pts:audio_end_pts;
        return f->best_effort_timestamp*av_q2d(fmt->streams[stream]->time_base);
    }
    bool open_decoder(AVCodecContext*& ctx,int index,bool video){
        if(index<0)return true;
        const AVStream* stream=fmt->streams[index];
        const AVCodec* codec=avcodec_find_decoder(stream->codecpar->codec_id);
        if(!codec){error="No decoder for selected stream";return false;}
        auto create=[&](bool gpu)->AVCodecContext* {
            AVCodecContext* out=avcodec_alloc_context3(codec);
            if(!out)return nullptr;
            if(avcodec_parameters_to_context(out,stream->codecpar)<0){
                avcodec_free_context(&out);return nullptr;
            }
            if(video){
                out->get_format=select_hw_or_cpu;
                if(gpu&&hw_device)out->hw_device_ctx=av_buffer_ref(hw_device);
            }
            if(avcodec_open2(out,codec,nullptr)<0){
                avcodec_free_context(&out);return nullptr;
            }
            return out;
        };
        if(video&&hw_device)ctx=create(true);
        if(!ctx)ctx=create(false);
        if(!ctx){error=std::string("Cannot initialize decoder: ")+codec->name;return false;}
        if(video){
            codec_name=codec->name;
            hw_active=ctx->hw_device_ctx!=nullptr;
            decoder_name=hw_active?"D3D11VA (requested)":"CPU";
        }
        return true;
    }
    bool open(const std::string& path,bool prefer_gpu,bool smoke,int limit,bool with_test_audio){
        source=path;test_mode=smoke;requested_frames=limit;smoke_audio=with_test_audio;
        if(!xs::native_is_local_media(path)||
           !std::filesystem::is_regular_file(path)){
            error="Local media file not found. No remote URLs allowed in pilot.";
            return false;
        }
        int rc=avformat_open_input(&fmt,path.c_str(),nullptr,nullptr);
        if(rc<0){error="Open media: "+av_err(rc);return false;}
        rc=avformat_find_stream_info(fmt,nullptr);
        if(rc<0){error="Find streams: "+av_err(rc);return false;}
        video_index=av_find_best_stream(fmt,AVMEDIA_TYPE_VIDEO,-1,-1,nullptr,0);
        audio_index=av_find_best_stream(fmt,AVMEDIA_TYPE_AUDIO,-1,-1,nullptr,0);
        if(video_index<0){error="File has no playable video stream.";return false;}
        for(unsigned i=0;i<fmt->nb_streams;++i){
            if(fmt->streams[i]->codecpar->codec_type==AVMEDIA_TYPE_AUDIO)
                audio_tracks.push_back(static_cast<int>(i));
        }
        std::filesystem::path subtitle_path(path);
        subtitle_path.replace_extension(".srt");
        sidecar_subtitles=xs::native_read_srt(subtitle_path.string());
        duration=fmt->duration>0?fmt->duration/double(AV_TIME_BASE):0;
        if(prefer_gpu&&supports_d3d11(avcodec_find_decoder(fmt->streams[video_index]->codecpar->codec_id))){
            if(av_hwdevice_ctx_create(&hw_device,AV_HWDEVICE_TYPE_D3D11VA,nullptr,nullptr,0)<0)
                hw_device=nullptr;
        }
        if(!open_decoder(vdec,video_index,true))return false;
        if((!test_mode||smoke_audio)&&audio_index>=0&&!open_decoder(adec,audio_index,false)){
            audio_index=-1;error.clear(); // video remains playable without audio
        }
        frame=av_frame_alloc();cpu_frame=av_frame_alloc();packet=av_packet_alloc();
        if(!frame||!cpu_frame||!packet){error="Out of memory for decoder frames.";return false;}
        SDL_SetMainReady();
        if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO|SDL_INIT_EVENTS)<0){
            error=std::string("SDL init: ")+SDL_GetError();return false;
        }
        did_sdl=true;
        SDL_SetHint(SDL_HINT_RENDER_DRIVER,smoke?"software":"direct3d11");
        src_w=vdec->width;src_h=vdec->height;
        if(src_w<1||src_h<1||src_w>8192||src_h>8192){
            error="Video dimensions exceed pilot limits.";return false;
        }
        window=SDL_CreateWindow("Xuper Native Player - Experimental",
            SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,
            std::clamp(src_w,640,1280),std::clamp(src_h,360,720),
            SDL_WINDOW_RESIZABLE|(smoke?SDL_WINDOW_HIDDEN:SDL_WINDOW_SHOWN));
        if(!window){error=std::string("Create video window: ")+SDL_GetError();return false;}
        if(!smoke)
            renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
        if(!renderer)renderer=SDL_CreateRenderer(window,-1,SDL_RENDERER_SOFTWARE);
        if(!renderer){error=std::string("SDL renderer: ")+SDL_GetError();return false;}
        SDL_RendererInfo info{};
        if(SDL_GetRendererInfo(renderer,&info)==0&&info.name)render_name=info.name;
        texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,
                                  SDL_TEXTUREACCESS_STREAMING,src_w,src_h);
        if(!texture){error=std::string("SDL texture: ")+SDL_GetError();return false;}
        SDL_SetTextureScaleMode(texture,SDL_ScaleModeLinear);
        pixels.resize(static_cast<size_t>(src_w)*src_h*4);
        if(!smoke&&TTF_Init()==0){
            did_ttf=true;
            char* root=SDL_GetBasePath();
            const std::string fontfile=std::string(root?root:"")+"font.ttf";
            if(root)SDL_free(root);
            font=TTF_OpenFont(fontfile.c_str(),18);
        }
        if(adec&&(!smoke||smoke_audio)){
            SDL_AudioSpec desired{},obtained{};
            desired.freq=sample_rate;
            desired.format=AUDIO_F32SYS;
            desired.channels=2;
            desired.samples=1024;
            desired.callback=nullptr;
            audio_device=SDL_OpenAudioDevice(nullptr,0,&desired,&obtained,0);
            if(audio_device){
                sample_rate=obtained.freq;
                SDL_PauseAudioDevice(audio_device,0);
            }else std::cerr<<"Audio unavailable, continuing silently: "<<SDL_GetError()<<"\n";
        }
        valid=true;
        return true;
    }
    void overlay_text(int x,int y,const std::string& str,SDL_Color color={235,239,247,255}){
        if(!font||str.empty())return;
        SDL_Surface* surface=TTF_RenderUTF8_Blended(font,str.c_str(),color);
        if(!surface)return;
        SDL_Texture* text=SDL_CreateTextureFromSurface(renderer,surface);
        if(text){
            SDL_Rect dest{x,y,surface->w,surface->h};
            SDL_RenderCopy(renderer,text,nullptr,&dest);
            SDL_DestroyTexture(text);
        }
        SDL_FreeSurface(surface);
    }
    double current_clock() const {
        if(audio_device&&has_audio_pts){
            const auto queued=SDL_GetQueuedAudioSize(audio_device);
            if(queued>0)return audio_end_pts-queued/double(sample_rate*2*sizeof(float));
        }
        if(!clock_started)return last_video_pts;
        return clock_origin+(SDL_GetTicks64()-tick_origin)/1000.0;
    }
    void render_last(double pts){
        if(!renderer||!texture)return;
        int w=0,h=0;
        SDL_GetRendererOutputSize(renderer,&w,&h);
        const auto fit=xs::native_aspect_fit(src_w,src_h,w,h);
        SDL_Rect rect{fit.x,fit.y,fit.w,fit.h};
        SDL_SetRenderDrawColor(renderer,7,9,14,255);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer,texture,nullptr,&rect);
        if(!test_mode&&subtitle_visible&&font){
            const auto caption=xs::native_subtitle_at(sidecar_subtitles,
                    static_cast<int64_t>(std::max(0.0,pts)*1000));
            if(!caption.empty()){
                SDL_Surface* sub=TTF_RenderUTF8_Blended_Wrapped(
                    font,caption.c_str(),SDL_Color{251,251,255,255},
                    static_cast<Uint32>(std::max(100,w-100)));
                if(sub){
                    SDL_Texture* sub_tex=SDL_CreateTextureFromSurface(renderer,sub);
                    if(sub_tex){
                        SDL_Rect dest{(w-sub->w)/2,std::max(86,h-76-sub->h),sub->w,sub->h};
                        SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
                        SDL_SetRenderDrawColor(renderer,0,0,0,195);
                        SDL_Rect shade{dest.x-12,dest.y-6,dest.w+24,dest.h+12};
                        SDL_RenderFillRect(renderer,&shade);
                        SDL_RenderCopy(renderer,sub_tex,nullptr,&dest);
                        SDL_DestroyTexture(sub_tex);
                    }
                    SDL_FreeSurface(sub);
                }
            }
        }
        if(!test_mode&&(verbose||paused)){
            SDL_SetRenderDrawBlendMode(renderer,SDL_BLENDMODE_BLEND);
            SDL_Rect panel{0,0,w,80};
            SDL_SetRenderDrawColor(renderer,5,8,13,198);
            SDL_RenderFillRect(renderer,&panel);
            std::string line="Xuper Native | "+codec_name+" | Decode: "+decoder_name+
                             " | Render: "+render_name;
            overlay_text(16,8,line);
            overlay_text(16,37,
                std::string(paused?"PAUSADO | ":"")+
                "Space pausa | flechas +/-10s | A audio | C subtitulos | F pantalla | F8 escala | I info");
            SDL_Rect bar{0,h-12,w,12};
            SDL_SetRenderDrawColor(renderer,26,32,48,228);
            SDL_RenderFillRect(renderer,&bar);
            if(duration>0){
                SDL_Rect done{0,h-12,
                    static_cast<int>(std::clamp(pts/duration,0.0,1.0)*w),12};
                SDL_SetRenderDrawColor(renderer,157,133,244,255);
                SDL_RenderFillRect(renderer,&done);
            }
        }
        if(test_mode&&!screenshot_path.empty()&&requested_frames>0&&
           rendered==requested_frames){
            SDL_Surface* capture=SDL_CreateRGBSurfaceWithFormat(
                0,w,h,32,SDL_PIXELFORMAT_ARGB8888);
            if(capture){
                screenshot_ok=SDL_RenderReadPixels(renderer,nullptr,
                        SDL_PIXELFORMAT_ARGB8888,capture->pixels,capture->pitch)==0&&
                    SDL_SaveBMP(capture,screenshot_path.c_str())==0;
                SDL_FreeSurface(capture);
            }
            if(!screenshot_ok)error=std::string("Frame screenshot: ")+SDL_GetError();
        }
        SDL_RenderPresent(renderer);
    }
    void pump_events(){
        SDL_Event ev;
        while(SDL_PollEvent(&ev)){
            if(ev.type==SDL_QUIT){running=false;return;}
            if(ev.type==SDL_KEYDOWN&&!ev.key.repeat){
                switch(ev.key.keysym.sym){
                    case SDLK_ESCAPE:running=false;break;
                    case SDLK_SPACE:
                        paused=!paused;
                        if(audio_device)SDL_PauseAudioDevice(audio_device,paused?1:0);
                        if(paused)tick_pause_started=SDL_GetTicks64();
                        else if(clock_started)tick_origin+=SDL_GetTicks64()-tick_pause_started;
                        break;
                    case SDLK_RIGHT:seek_to=xs::native_clamp_seek(last_video_pts+10,duration);break;
                    case SDLK_LEFT:seek_to=xs::native_clamp_seek(last_video_pts-10,duration);break;
                    case SDLK_PAGEUP:seek_to=xs::native_clamp_seek(last_video_pts+60,duration);break;
                    case SDLK_PAGEDOWN:seek_to=xs::native_clamp_seek(last_video_pts-60,duration);break;
                    case SDLK_HOME:seek_to=0;break;
                    case SDLK_UP:volume=xs::native_clamp_volume(volume+5);break;
                    case SDLK_DOWN:volume=xs::native_clamp_volume(volume-5);break;
                    case SDLK_i:verbose=!verbose;break;
                    case SDLK_c:subtitle_visible=!subtitle_visible;break;
                    case SDLK_a:cycle_audio_track();break;
                    case SDLK_f:
                    case SDLK_F11:
                        fullscreen=!fullscreen;
                        SDL_SetWindowFullscreen(window,fullscreen?SDL_WINDOW_FULLSCREEN_DESKTOP:0);
                        break;
                    case SDLK_F8:
                        nearest=!nearest;
                        if(texture)SDL_SetTextureScaleMode(texture,
                             nearest?SDL_ScaleModeNearest:SDL_ScaleModeLinear);
                        break;
                    default:break;
                }
            }
            if(ev.type==SDL_MOUSEBUTTONDOWN&&ev.button.button==SDL_BUTTON_LEFT){
                int w=0,h=0;
                SDL_GetWindowSize(window,&w,&h);
                if(duration>0&&ev.button.y>=h-27&&w>0)
                    seek_to=xs::native_clamp_seek(duration*ev.button.x/double(w),duration);
            }
        }
    }
    void cycle_audio_track(){
        if(audio_tracks.size()<2||!audio_device)return;
        const auto current=std::find(audio_tracks.begin(),audio_tracks.end(),audio_index);
        const size_t index=current==audio_tracks.end()?0:
            (static_cast<size_t>(current-audio_tracks.begin())+1)%audio_tracks.size();
        const int selected=audio_tracks[index];
        AVCodecContext* replacement=nullptr;
        if(!open_decoder(replacement,selected,false)){
            error.clear();
            std::cerr<<"Audio track "<<selected<<" unsupported.\n";
            return;
        }
        if(adec)avcodec_free_context(&adec);
        adec=replacement;
        audio_index=selected;
        seek_to=last_video_pts;
    }
    bool do_seek(){
        const double target=seek_to;
        seek_to=-1;
        if(target<0||video_index<0)return true;
        const int64_t timestamp=static_cast<int64_t>(
            target/av_q2d(fmt->streams[video_index]->time_base));
        const int rc=av_seek_frame(fmt,video_index,timestamp,AVSEEK_FLAG_BACKWARD);
        if(rc<0){
            std::cerr<<"Seek unavailable: "<<av_err(rc)<<"\n";
            return true;
        }
        avcodec_flush_buffers(vdec);
        if(adec)avcodec_flush_buffers(adec);
        if(swr)swr_free(&swr);
        audio_input_rate=0;audio_input_fmt=-1;audio_input_channels=0;
        if(audio_device)SDL_ClearQueuedAudio(audio_device);
        clock_started=false;has_audio_pts=false;
        audio_end_pts=target;last_video_pts=target;
        return true;
    }
    bool prepare_audio(const AVFrame* f){
        if(!audio_device)return true;
        if(f->sample_rate<1||f->ch_layout.nb_channels<1)return false;
        const bool changed=!swr||f->sample_rate!=audio_input_rate||
             f->format!=audio_input_fmt||f->ch_layout.nb_channels!=audio_input_channels;
        if(changed){
            if(swr)swr_free(&swr);
            const AVChannelLayout stereo=AV_CHANNEL_LAYOUT_STEREO;
            const int rc=swr_alloc_set_opts2(&swr,&stereo,AV_SAMPLE_FMT_FLT,
                sample_rate,&f->ch_layout,static_cast<AVSampleFormat>(f->format),
                f->sample_rate,0,nullptr);
            if(rc<0||!swr||swr_init(swr)<0){error="Audio resampling setup failed.";return false;}
            audio_input_rate=f->sample_rate;
            audio_input_fmt=f->format;
            audio_input_channels=f->ch_layout.nb_channels;
        }
        return true;
    }
    bool process_audio(AVFrame* f){
        if(!audio_device||(test_mode&&!smoke_audio))return true;
        if(!prepare_audio(f))return false;
        const int possible=static_cast<int>(av_rescale_rnd(
            swr_get_delay(swr,f->sample_rate)+f->nb_samples,
            sample_rate,f->sample_rate,AV_ROUND_UP));
        if(possible<=0||possible>sample_rate*4){error="Invalid audio sample count.";return false;}
        std::vector<float> stereo(static_cast<size_t>(possible)*2);
        uint8_t* dest=reinterpret_cast<uint8_t*>(stereo.data());
        int samples=swr_convert(swr,&dest,possible,
            const_cast<const uint8_t**>(f->extended_data),f->nb_samples);
        if(samples<0){error="Resampling: "+av_err(samples);return false;}
        audio_samples_decoded+=samples;
        const size_t floats=static_cast<size_t>(samples)*2;
        const float gain=volume/100.f;
        for(size_t i=0;i<floats;++i)stereo[i]*=gain;
        const double pts=frame_pts(f,audio_index);
        audio_end_pts=pts+samples/double(sample_rate);
        has_audio_pts=true;
        const Uint32 bytes=static_cast<Uint32>(floats*sizeof(float));
        if(SDL_QueueAudio(audio_device,stereo.data(),bytes)<0){
            error=std::string("SDL audio: ")+SDL_GetError();return false;
        }
        // Bound buffered audio to avoid multi-second A/V drift and memory
        // growth on files with unusually dense audio packet interleaving.
        while(!test_mode&&running&&!paused&&seek_to<0&&
              SDL_GetQueuedAudioSize(audio_device)>static_cast<Uint32>(sample_rate*2*sizeof(float))){
            pump_events();
            SDL_Delay(5);
        }
        return true;
    }
    bool present_video(AVFrame* decoded){
        const AVFrame* visual=decoded;
        if(decoded->format==AV_PIX_FMT_D3D11){
            av_frame_unref(cpu_frame);
            int rc=av_hwframe_transfer_data(cpu_frame,decoded,0);
            if(rc<0){error="D3D11VA transfer: "+av_err(rc);return false;}
            visual=cpu_frame;
            hw_active=true;
            decoder_name="D3D11VA";
        }else{
            if(!hw_active)decoder_name="CPU";
        }
        if(visual->width<=0||visual->height<=0||visual->width>8192||visual->height>8192){
            error="Invalid decoded dimensions.";return false;
        }
        if(src_w!=visual->width||src_h!=visual->height){
            src_w=visual->width;src_h=visual->height;
            if(texture)SDL_DestroyTexture(texture);
            texture=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_ARGB8888,
                      SDL_TEXTUREACCESS_STREAMING,src_w,src_h);
            if(!texture){error=std::string("Resize GPU texture: ")+SDL_GetError();return false;}
            SDL_SetTextureScaleMode(texture,nearest?SDL_ScaleModeNearest:SDL_ScaleModeLinear);
            pixels.resize(static_cast<size_t>(src_w)*src_h*4);
        }
        sws=sws_getCachedContext(sws,src_w,src_h,
            static_cast<AVPixelFormat>(visual->format),
            src_w,src_h,AV_PIX_FMT_BGRA,SWS_BICUBIC,nullptr,nullptr,nullptr);
        if(!sws){error="FFmpeg pixel conversion unavailable.";return false;}
        uint8_t* destination[]={pixels.data()};
        int stride[]={src_w*4};
        if(sws_scale(sws,visual->data,visual->linesize,0,src_h,destination,stride)<0){
            error="Could not convert video pixels.";return false;
        }
        double pts=frame_pts(decoded,video_index);
        if(decoded->best_effort_timestamp==AV_NOPTS_VALUE)
            pts=last_video_pts+1.0/30.0;
        last_video_pts=pts;
        if(!clock_started){
            clock_started=true;
            clock_origin=pts;
            tick_origin=SDL_GetTicks64();
        }
        if(!test_mode){
            while(running&&!paused&&seek_to<0&&pts-current_clock()>0.008){
                pump_events();
                SDL_Delay(3);
            }
            if(seek_to>=0||!running)return true;
            if(pts-current_clock() < -0.20){
                ++dropped;return true; // drop stale frame, not audio
            }
        }
        if(SDL_UpdateTexture(texture,nullptr,pixels.data(),src_w*4)<0){
            error=std::string("Upload video texture: ")+SDL_GetError();return false;
        }
        ++rendered;
        render_last(pts);
        if(requested_frames>0&&rendered>=requested_frames)running=false;
        return true;
    }
    bool drain(AVCodecContext* decoder,bool is_video){
        if(!decoder)return true;
        while(running&&seek_to<0){
            int rc=avcodec_receive_frame(decoder,frame);
            if(rc==AVERROR(EAGAIN)||rc==AVERROR_EOF)return true;
            if(rc<0){error="Decode frame: "+av_err(rc);return false;}
            const bool ok=is_video?present_video(frame):process_audio(frame);
            av_frame_unref(frame);
            if(!ok)return false;
        }
        return true;
    }
    bool send(AVCodecContext* decoder,const AVPacket* pkt,bool video){
        if(!decoder)return true;
        int rc=avcodec_send_packet(decoder,pkt);
        if(rc==AVERROR(EAGAIN)){
            if(!drain(decoder,video))return false;
            rc=avcodec_send_packet(decoder,pkt);
        }
        if(rc<0&&rc!=AVERROR_EOF){error="Send packet: "+av_err(rc);return false;}
        return drain(decoder,video);
    }
    int run(){
        if(!valid)return 2;
        int read_packets=0;
        while(running){
            pump_events();
            if(seek_to>=0){if(!do_seek())return 1;continue;}
            if(paused){render_last(last_video_pts);SDL_Delay(40);continue;}
            int rc=av_read_frame(fmt,packet);
            if(rc==AVERROR_EOF)break;
            if(rc<0){error="Read packet: "+av_err(rc);return 1;}
            if(packet->stream_index==video_index){
                if(!send(vdec,packet,true)){av_packet_unref(packet);return 1;}
            }else if(packet->stream_index==audio_index&&adec){
                if(!send(adec,packet,false)){av_packet_unref(packet);return 1;}
            }
            av_packet_unref(packet);
            if(++read_packets>2000000){error="Packet processing limit exceeded.";return 1;}
        }
        if(running&&seek_to<0){
            if(!send(vdec,nullptr,true))return 1;
            if(adec&&!send(adec,nullptr,false))return 1;
        }
        if(!test_mode&&audio_device&&running){
            while(running&&SDL_GetQueuedAudioSize(audio_device)>0){
                pump_events();
                if(seek_to>=0)break; // seeking near end returns to caller
                SDL_Delay(10);
            }
        }
        std::cout<<"XuperNativePlayer: rendered="<<rendered
                 <<" dropped="<<dropped
                 <<" decoder="<<decoder_name
                 <<" renderer="<<render_name
                 <<" audio="<<(audio_device?"SDL queued stereo":"off")
                 <<" audio_samples="<<audio_samples_decoded
                 <<" frame_size="<<src_w<<"x"<<src_h<<"\n";
        if(test_mode&&smoke_audio&&audio_samples_decoded==0){
            error="Audio smoke requested but no PCM samples were decoded.";
            return 1;
        }
        return rendered>0&&(screenshot_path.empty()||screenshot_ok)?0:1;
    }
};
} // namespace

int main(int argc,char** argv){
    SDL_SetMainReady();
    if(argc<2){
        std::cout<<"XuperNativePlayer <local-video-file> [--cpu] [--smoke N] [--screenshot bmp]\n"
                 <<"  Space pause | arrows seek | Up/Down volume | F fullscreen\n"
                 <<"  F8 nearest/linear GPU scaler | I status | Esc exit\n";
        return 2;
    }
    const std::string file=argv[1];
    bool hardware=true,smoke=false,audio_smoke=false;
    int frames=0;
    std::string screenshot;
    for(int i=2;i<argc;++i){
        const std::string arg=argv[i];
        if(arg=="--cpu")hardware=false;
        else if(arg=="--smoke-audio")audio_smoke=true;
        else if(arg=="--screenshot"&&i+1<argc)screenshot=argv[++i];
        else if(arg=="--smoke"&&i+1<argc){
            smoke=true;
            try{frames=std::stoi(argv[++i]);}catch(...){return 2;}
            if(frames<1||frames>300){std::cerr<<"Invalid smoke frame count.\n";return 2;}
        }else {std::cerr<<"Unknown argument: "<<arg<<"\n";return 2;}
    }
    if(audio_smoke&&!smoke){std::cerr<<"--smoke-audio requires --smoke N.\n";return 2;}
    if(!smoke&&!screenshot.empty()){
        std::cerr<<"Screenshot export is only available in smoke-test mode.\n";
        return 2;
    }
    NativePlayer player;
    player.screenshot_path=screenshot;
    if(!player.open(file,hardware,smoke,frames,audio_smoke)){
        std::cerr<<"Native player start failed: "<<player.error<<"\n";
        return 1;
    }
    const int code=player.run();
    if(code!=0)std::cerr<<"Native player error: "<<player.error<<"\n";
    return code;
}
