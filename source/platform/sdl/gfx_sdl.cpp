#ifdef BACKEND_SDL

#include <malloc.h>

#include <sstream>

#include "platform/gfx.h"
#include "platform/input.h"

#include <SDL2/SDL.h>

static SDL_Rect windowRect = {0, 0, 160, 144};
static SDL_Rect textureRect = {0, 0, 160, 144};

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Texture* texture = NULL;

static u32* screenBuffer;

static bool fastForward = false;

bool gfxInit() {
    window = SDL_CreateWindow("GameYob", 0, 0, 160, 144, SDL_WINDOW_RESIZABLE);
    if(window == NULL) {
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
    if(renderer == NULL) {
        return false;
    }

    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 160, 144);
    if(texture == NULL) {
        return false;
    }

    screenBuffer = (u32*) malloc(160 * 144 * sizeof(u32));

    return true;
}

void gfxCleanup() {
    if(screenBuffer != NULL) {
        free(screenBuffer);
        screenBuffer = NULL;
    }

    if(texture != NULL) {
        SDL_DestroyTexture(texture);
        texture = NULL;
    }

    if(renderer != NULL) {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }

    if(window != NULL) {
        SDL_DestroyWindow(window);
        window = NULL;
    }
}

void gfxSetWindowSize(int width, int height) {
    windowRect.w = width;
    windowRect.h = height;
}

bool gfxGetFastForward() {
    return fastForward || inputKeyHeld(FUNC_KEY_FAST_FORWARD);
}

void gfxSetFastForward(bool fastforward) {
    fastForward = fastforward;
}

void gfxToggleFastForward() {
    fastForward = !fastForward;
}

void gfxLoadBorder(u8* imgData, u32 imgWidth, u32 imgHeight) {
}

u32* gfxGetLineBuffer(int line) {
    return screenBuffer + line * 160;
}

void gfxClearScreenBuffer(u8 r, u8 g, u8 b) {
    if(r == g && r == b) {
        memset(screenBuffer, r, 160 * 144 * sizeof(u32));
    } else {
        wmemset((wchar_t*) screenBuffer, (wchar_t) RGBA32(r, g, b), 160 * 144);
    }
}

void gfxDrawScreen() {
    SDL_UpdateTexture(texture, &textureRect, screenBuffer, 160 * 4);
    SDL_RenderCopy(renderer, texture, &textureRect, &windowRect);
    SDL_RenderPresent(renderer);

    if(inputKeyPressed(FUNC_KEY_SCREENSHOT)) {
        std::stringstream fileStream;
        fileStream << "screenshot_" << time(NULL) << ".bmp";

        SDL_Surface* screenshot = SDL_CreateRGBSurface(0, 160, 144, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
        SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, screenshot->pixels, screenshot->pitch);
        SDL_SaveBMP(screenshot, fileStream.str().c_str());
        SDL_FreeSurface(screenshot);
    }

    // TODO: Fast-forward
}

void gfxWaitForVBlank() {
    SDL_RenderPresent(renderer);
}

#endif