#include "gpu_readback.hpp"
#ifdef _WIN32
#include <mpv/render_gl.h>
#include <SDL_opengl.h>
#include <cstring>

namespace xs {
namespace {
void* get_gl_proc(void*,const char* name){
    return SDL_GL_GetProcAddress(name);
}
}
GpuReadback::~GpuReadback(){close();}

bool GpuReadback::make_current(){
    return window_&&gl_&&SDL_GL_MakeCurrent(window_,gl_)==0;
}

bool GpuReadback::open(mpv_handle* mpv,int width,int height,std::string& diagnostic){
    if(!mpv||width<=0||height<=0||context_){diagnostic="Parametros GPU invalidos.";return false;}
    width_=width;height_=height;
    // This is a *separate* hidden window: the public SDL2 renderer and
    // video controls remain unchanged, even on a failed OpenGL attempt.
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);
    window_=SDL_CreateWindow("XuperTV GPU offscreen",
        SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,width,height,
        SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    if(!window_){diagnostic=std::string("SDL OpenGL window: ")+SDL_GetError();close();return false;}
    gl_=SDL_GL_CreateContext(window_);
    if(!gl_||!make_current()){
        diagnostic=std::string("SDL OpenGL context: ")+SDL_GetError();
        close();return false;
    }
    const char* version=reinterpret_cast<const char*>(glGetString(GL_VERSION));
    if(!version){diagnostic="OpenGL version unavailable";close();return false;}
    mpv_opengl_init_params gl_init{};
    gl_init.get_proc_address=get_gl_proc;
    gl_init.get_proc_address_ctx=nullptr;
    mpv_render_param init[]={
        {MPV_RENDER_PARAM_API_TYPE,const_cast<char*>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS,&gl_init},
        {MPV_RENDER_PARAM_INVALID,nullptr}
    };
    const int rc=mpv_render_context_create(&context_,mpv,init);
    if(rc<0){
        diagnostic=std::string("mpv GPU renderer: ")+mpv_error_string(rc);
        close();return false;
    }
    diagnostic=std::string("OpenGL ")+version+" | readback a SDL";
    return true;
}

bool GpuReadback::render(void* rgba_pixels,std::string& diagnostic){
    if(!context_||!rgba_pixels||!make_current()){
        diagnostic="Contexto OpenGL no disponible.";
        return false;
    }
    mpv_opengl_fbo target{};
    target.fbo=0;target.w=width_;target.h=height_;target.internal_format=0;
    // OpenGL's default framebuffer is bottom-up; flip in mpv so the
    // buffer returned by glReadPixels is top-down for SDL's streaming texture.
    int flip=1;
    mpv_render_param params[]={
        {MPV_RENDER_PARAM_OPENGL_FBO,&target},
        {MPV_RENDER_PARAM_FLIP_Y,&flip},
        {MPV_RENDER_PARAM_INVALID,nullptr}
    };
    const int rc=mpv_render_context_render(context_,params);
    if(rc<0){diagnostic=mpv_error_string(rc);return false;}
    glPixelStorei(GL_PACK_ALIGNMENT,4);
    glReadBuffer(GL_BACK);
    glReadPixels(0,0,width_,height_,GL_RGBA,GL_UNSIGNED_BYTE,rgba_pixels);
    GLenum gl_err=glGetError();
    if(gl_err!=GL_NO_ERROR){
        diagnostic="OpenGL readback failed: "+std::to_string(static_cast<unsigned>(gl_err));
        return false;
    }
    return true;
}
void GpuReadback::close(){
    // libmpv requires this exact GL context to remain current until
    // render context teardown; never destroy the GL objects first.
    if(context_){
        make_current();
        mpv_render_context_free(context_);
        context_=nullptr;
    }
    if(gl_){
        SDL_GL_DeleteContext(gl_);gl_=nullptr;
    }
    if(window_){
        SDL_DestroyWindow(window_);window_=nullptr;
    }
}
}
#endif
