#ifdef BACKEND_SDL

#include <malloc.h>

#include <sstream>

#include "platform/gfx.h"
#include "platform/input.h"

#include <SDL2/SDL.h>

static SDL_Rect windowRect = {0, 0, 160, 144};
static SDL_Rect screenRect = {0, 0, 160, 144};

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Texture* screenTexture = NULL;

static u16* screenBuffer;

static bool fastForward = false;

bool gfxInit() {
    window = SDL_CreateWindow("GameYob", 0, 0, 160, 144, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
    if(window == NULL) {
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, 0);
    if(renderer == NULL) {
        return false;
    }

    screenTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA5551, SDL_TEXTUREACCESS_STREAMING, 160, 144);
    if(screenTexture == NULL) {
        return false;
    }

    screenBuffer = (u16*) malloc(160 * 144 * sizeof(u16));

    return true;
}

void gfxCleanup() {
    if(screenBuffer != NULL) {
        free(screenBuffer);
        screenBuffer = NULL;
    }

    if(screenTexture != NULL) {
        SDL_DestroyTexture(screenTexture);
        screenTexture = NULL;
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

u16* gfxGetLineBuffer(u32 line) {
    return screenBuffer + line * 160;
}

void gfxClearScreenBuffer(u16 rgba5551) {
    if(((rgba5551 >> 8) & 0xFF) == (rgba5551 & 0xFF)) {
        memset(screenBuffer, rgba5551 & 0xFF, 160 * 144 * sizeof(u16));
    } else {
        for(int i = 0; i < 160 * 144; i++) {
            screenBuffer[i] = rgba5551;
        }
    }
}

void gfxClearLineBuffer(u32 line, u16 rgba5551) {
    u16* lineBuffer = gfxGetLineBuffer(line);
    if(((rgba5551 >> 8) & 0xFF) == (rgba5551 & 0xFF)) {
        memset(lineBuffer, rgba5551 & 0xFF, 160 * sizeof(u16));
    } else {
        for(int i = 0; i < 160; i++) {
            lineBuffer[i] = rgba5551;
        }
    }
}

void gfxDrawScreen() {
    SDL_GL_SetSwapInterval(!gfxGetFastForward());

    SDL_UpdateTexture(screenTexture, &screenRect, screenBuffer, 160 * 2);
    SDL_RenderCopy(renderer, screenTexture, &screenRect, &windowRect);
    SDL_RenderPresent(renderer);

    if(inputKeyPressed(FUNC_KEY_SCREENSHOT)) {
        std::stringstream fileStream;
        fileStream << "screenshot_" << time(NULL) << ".bmp";

        SDL_Surface* screenshot = SDL_CreateRGBSurface(0, 160, 144, 32, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
        SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, screenshot->pixels, screenshot->pitch);
        SDL_SaveBMP(screenshot, fileStream.str().c_str());
        SDL_FreeSurface(screenshot);
    }
}

void gfxSync() {
    SDL_GL_SetSwapInterval(true);
    SDL_RenderPresent(renderer);
}

#endif