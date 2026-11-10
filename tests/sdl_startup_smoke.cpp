#include <SDL.h>
#include <cstdio>
#undef main

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    // Executed from the packaged artifact with only Windows system folders
    // on PATH, so all non-system SDL dependencies must be bundled beside it.
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::fprintf(stderr, "SDL startup failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window* window=SDL_CreateWindow("Xuper portable smoke test",
        SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,160,90,SDL_WINDOW_HIDDEN);
    if (!window) {
        std::fprintf(stderr, "SDL window failed: %s\n",SDL_GetError());
        SDL_Quit();
        return 2;
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    std::puts("Portable SDL2 compatibility / SDL3 runtime startup OK.");
    return 0;
}
