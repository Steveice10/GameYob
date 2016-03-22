#ifdef BACKEND_SDL

#include <malloc.h>
#include <time.h>

#include <sstream>

#include "platform/gfx.h"
#include "platform/input.h"

#include <SDL2/SDL.h>

static SDL_Rect screenRect = {0, 0, 160, 144};
static SDL_Rect windowScreenRect = {0, 0, 160, 144};

static SDL_Rect borderRect = {0, 0, 0, 0};
static SDL_Rect windowBorderRect = {0, 0, 0, 0};

static SDL_Window* window = NULL;
static SDL_Renderer* renderer = NULL;
static SDL_Texture* screenTexture = NULL;
static SDL_Texture* borderTexture = NULL;

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

    if(borderTexture != NULL) {
        SDL_DestroyTexture(borderTexture);
        borderTexture = NULL;
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

void gfxUpdateWindow() {
    int w;
    int h;
    SDL_GetWindowSize(window, &w, &h);

    if(borderTexture != NULL) {
        if(w == 160 && h == 144) {
            SDL_SetWindowSize(window, borderRect.w, borderRect.h);
            w = borderRect.w;
            h = borderRect.h;
        }

        windowBorderRect.w = w;
        windowBorderRect.h = h;
        windowBorderRect.x = 0;
        windowBorderRect.y = 0;

        windowScreenRect.w = (int) (w * (160.0f / (float) borderRect.w));
        windowScreenRect.h = (int) (h * (144.0f / (float) borderRect.h));
        windowScreenRect.x = (w - windowScreenRect.w) / 2;
        windowScreenRect.y = (h - windowScreenRect.h) / 2;
    } else {
        if(w == borderRect.w && h == borderRect.h) {
            SDL_SetWindowSize(window, 160, 144);
            w = 160;
            h = 144;
        }

        windowScreenRect.w = w;
        windowScreenRect.h = h;
        windowScreenRect.x = 0;
        windowScreenRect.y = 0;
    }
}

void gfxSetWindowSize(int width, int height) {
    gfxUpdateWindow();
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

void gfxLoadBorder(u8* imgData, int imgWidth, int imgHeight) {
    if(borderTexture != NULL) {
        SDL_DestroyTexture(borderTexture);
        borderTexture = NULL;
    }

    if(imgData != NULL && imgWidth != 0 && imgHeight != 0) {
        borderTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, imgWidth, imgHeight);
        if(borderTexture == NULL) {
            return;
        }

        SDL_SetTextureBlendMode(borderTexture, SDL_BLENDMODE_BLEND);

        borderRect.w = imgWidth;
        borderRect.h = imgHeight;

        SDL_UpdateTexture(borderTexture, &borderRect, imgData, imgWidth * sizeof(u32));
    }

    gfxUpdateWindow();
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

void gfxTakeScreenshot() {
    std::stringstream fileStream;
    fileStream << "gameyob_" << time(NULL) << ".bmp";

    int w;
    int h;
    SDL_GetWindowSize(window, &w, &h);

    SDL_Surface *surface = SDL_CreateRGBSurface(0, w, h, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
    SDL_RenderReadPixels(renderer, NULL, SDL_PIXELFORMAT_ARGB8888, surface->pixels, surface->pitch);
    SDL_SaveBMP(surface, fileStream.str().c_str());
    SDL_FreeSurface(surface);
}

void gfxDrawScreen() {
    SDL_GL_SetSwapInterval(!gfxGetFastForward());

    SDL_RenderClear(renderer);

    SDL_UpdateTexture(screenTexture, &screenRect, screenBuffer, 160 * 2);
    SDL_RenderCopy(renderer, screenTexture, &screenRect, &windowScreenRect);

    if(borderTexture != NULL) {
        SDL_RenderCopy(renderer, borderTexture, &borderRect, &windowBorderRect);
    }

    SDL_RenderPresent(renderer);
}

void gfxSync() {
    SDL_GL_SetSwapInterval(true);
    SDL_RenderPresent(renderer);
}

#endif