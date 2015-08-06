#include <malloc.h>
#include <math.h>

#include "lodepng/lodepng.h"
#include "platform/gfx.h"
#include "platform/input.h"
#include "ui/config.h"
#include "ui/menu.h"

#include <3ds.h>

#include <ctrcommon/gpu.hpp>

static u32* screenBuffer;

static int prevScaleMode = -1;
static int prevScaleFilter = -1;
static int prevGameScreen = -1;

static bool fastForward = false;

static u32 texture = 0;
static u32 vbo = 0;

static u32 borderVbo = 0;
static u32 borderTexture = 0;

static u32 borderWidth = 0;
static u32 borderHeight = 0;
static u32 gpuBorderWidth = 0;
static u32 gpuBorderHeight = 0;

bool gfxInit() {
    // Allocate and clear the screen buffer.
    screenBuffer = (u32*) gpuAlloc(256 * 256 * sizeof(u32));
    memset(screenBuffer, 0, 256 * 256 * sizeof(u32));

    // Setup the GPU state.
    gpuCullMode(CULL_BACK_CCW);

    // Create the VBO.
    gpuCreateVbo(&vbo);
    gpuVboAttributes(vbo, ATTRIBUTE(0, 3, ATTR_FLOAT) | ATTRIBUTE(1, 2, ATTR_FLOAT) | ATTRIBUTE(2, 4, ATTR_FLOAT), 3);

    // Create the texture.
    gpuCreateTexture(&texture);

    return true;
}

void gfxCleanup() {
    // Free texture.
    if(texture != 0) {
        gpuFreeTexture(texture);
        texture = 0;
    }

    // Free VBO.
    if(vbo != 0) {
        gpuFreeVbo(vbo);
        vbo = 0;
    }

    // Free border texture.
    if(borderTexture != 0) {
        gpuFreeTexture(borderTexture);
        borderTexture = 0;
    }

    // Free border VBO.
    if(borderVbo != 0) {
        gpuFreeVbo(borderVbo);
        borderVbo = 0;
    }

    // Free screen buffer.
    if(screenBuffer != NULL) {
        gpuFree(screenBuffer);
        screenBuffer = NULL;
    }
}

bool gfxGetFastForward() {
    return fastForward || inputKeyHeld(inputMapFuncKey(FUNC_KEY_FAST_FORWARD));
}

void gfxSetFastForward(bool fastforward) {
    fastForward = fastforward;
}

void gfxToggleFastForward() {
    fastForward = !fastForward;
}

void gfxLoadBorder(const char* filename) {
    if(filename == NULL) {
        gfxLoadBorderBuffer(NULL, 0, 0);
        return;
    }

    // Load the image.
    unsigned char* imgData;
    unsigned int imgWidth;
    unsigned int imgHeight;
    if(lodepng_decode32_file(&imgData, &imgWidth, &imgHeight, filename)) {
        return;
    }

    gfxLoadBorderBuffer(imgData, imgWidth, imgHeight);
    free(imgData);
}

void gfxLoadBorderBuffer(u8* imgData, u32 imgWidth, u32 imgHeight) {
    if(imgData == NULL) {
        if(borderTexture != 0) {
            gpuFreeTexture(borderTexture);
            borderTexture = 0;
        }

        if(borderVbo != 0) {
            gpuFreeVbo(borderVbo);
            borderVbo = 0;
        }

        borderWidth = 0;
        borderHeight = 0;
        gpuBorderWidth = 0;
        gpuBorderHeight = 0;

        return;
    }

    // Adjust the texture to power-of-two dimensions.
    borderWidth = imgWidth;
    borderHeight = imgHeight;
    gpuBorderWidth = (u32) pow(2, ceil(log(borderWidth) / log(2)));
    gpuBorderHeight = (u32) pow(2, ceil(log(borderHeight) / log(2)));
    u8* borderBuffer = (u8*) gpuAlloc(gpuBorderWidth * gpuBorderHeight * 4);
    for(u32 x = 0; x < borderWidth; x++) {
        for(u32 y = 0; y < borderHeight; y++) {
            borderBuffer[(y * gpuBorderWidth + x) * 4 + 0] = imgData[(y * borderWidth + x) * 4 + 3];
            borderBuffer[(y * gpuBorderWidth + x) * 4 + 1] = imgData[(y * borderWidth + x) * 4 + 2];
            borderBuffer[(y * gpuBorderWidth + x) * 4 + 2] = imgData[(y * borderWidth + x) * 4 + 1];
            borderBuffer[(y * gpuBorderWidth + x) * 4 + 3] = imgData[(y * borderWidth + x) * 4 + 0];
        }
    }

    // Create the texture.
    if(borderTexture == 0) {
        gpuCreateTexture(&borderTexture);
    }

    // Update texture data.
    gpuTextureData(borderTexture, borderBuffer, gpuBorderWidth, gpuBorderHeight, PIXEL_RGBA8, TEXTURE_MIN_FILTER(FILTER_LINEAR) | TEXTURE_MAG_FILTER(FILTER_LINEAR));

    // Free the buffer.
    gpuFree(borderBuffer);
}

u32* gfxGetLineBuffer(int line) {
    return screenBuffer + line * 256;
}

void gfxClearScreenBuffer(u8 r, u8 g, u8 b) {
    if(r == g && r == b) {
        memset(screenBuffer, r, 256 * 256 * sizeof(u32));
    } else {
        wmemset((wchar_t*) screenBuffer, (wchar_t) RGBA32(r, g, b), 256 * 256);
    }
}

#define PIXEL_AT( PX, XX, YY) ((PX)+((((YY)*256)+(XX))))
#define SCALE2XMACRO()       if (Bp!=Hp && Dp!=Fp) {        \
                                if (Dp==Bp) *(E0++) = Dp;   \
                                else *(E0++) = Ep;          \
                                if (Bp==Fp) *(E0++) = Fp;   \
                                else *(E0++) = Ep;          \
                                if (Dp==Hp) *(E0++) = Dp;   \
                                else *(E0++) = Ep;          \
                                if (Hp==Fp) *E0 = Fp;       \
                                else *E0 = Ep;              \
                             } else {                       \
                                *(E0++) = Ep;               \
                                *(E0++) = Ep;               \
                                *(E0++) = Ep;               \
                                *E0 = Ep;                   \
                             }

void gfxScale2x(u32* pixelBuffer) {
    int x, y;
    u32* E, * E0;
    u32 Ep, Bp, Dp, Fp, Hp;
    u32* buffer = (u32*) gpuGetTextureData(texture);

    // Top line and top corners
    E = PIXEL_AT(pixelBuffer, 0, 0);
    E0 = buffer + gpuTextureIndex(0, 0, 512, 512);
    Ep = E[0];
    Bp = Ep;
    Dp = Ep;
    Fp = E[1];
    Hp = E[256];
    SCALE2XMACRO();
    for(x = 1; x < 159; x++) {
        E += 1;
        Dp = Ep;
        Ep = Fp;
        Fp = E[1];
        Bp = Ep;
        Hp = E[256];
        E0 = buffer + gpuTextureIndex((u32) (x * 2), 0, 512, 512);
        SCALE2XMACRO();
    }

    E += 1;
    Dp = Ep;
    Ep = Fp;
    Bp = Ep;
    Hp = E[256];
    E0 = buffer + gpuTextureIndex(159 * 2, 0, 512, 512);
    SCALE2XMACRO();

    // Middle Rows and sides
    for(y = 1; y < 143; y++) {
        E = PIXEL_AT(pixelBuffer, 0, y);
        E0 = buffer + gpuTextureIndex(0, (u32) (y * 2), 512, 512);
        Ep = E[0];
        Bp = E[-256];
        Dp = Ep;
        Fp = E[1];
        Hp = E[256];
        SCALE2XMACRO();
        for(x = 1; x < 159; x++) {
            E += 1;
            Dp = Ep;
            Ep = Fp;
            Fp = E[1];
            Bp = E[-256];
            Hp = E[256];
            E0 = buffer + gpuTextureIndex((u32) (x * 2), (u32) (y * 2), 512, 512);
            SCALE2XMACRO();
        }
        E += 1;
        Dp = Ep;
        Ep = Fp;
        Bp = E[-256];
        Hp = E[256];
        E0 = buffer + gpuTextureIndex(159 * 2, (u32) (y * 2), 512, 512);
        SCALE2XMACRO();
    }

    // Bottom Row and Bottom Corners
    E = PIXEL_AT(pixelBuffer, 0, 143);
    E0 = buffer + gpuTextureIndex(0, 143 * 2, 512, 512);
    Ep = E[0];
    Bp = E[-256];
    Dp = Ep;
    Fp = E[1];
    Hp = Ep;
    SCALE2XMACRO();
    for(x = 1; x < 159; x++) {
        E += 1;
        Dp = Ep;
        Ep = Fp;
        Fp = E[1];
        Bp = E[-256];
        Hp = Ep;
        E0 = buffer + gpuTextureIndex((u32) (x * 2), 143 * 2, 512, 512);
        SCALE2XMACRO();
    }

    E += 1;
    Dp = Ep;
    Ep = Fp;
    Bp = E[-256];
    Hp = Ep;
    E0 = buffer + gpuTextureIndex(159 * 2, 143 * 2, 512, 512);
    SCALE2XMACRO();
}

void gfxDrawScreen() {
    // Update the border VBO.
    if(borderTexture != 0 && (borderVbo == 0 || prevGameScreen != gameScreen)) {
        // Create the VBO.
        if(borderVbo == 0) {
            gpuCreateVbo(&borderVbo);
            gpuVboAttributes(borderVbo, ATTRIBUTE(0, 3, ATTR_FLOAT) | ATTRIBUTE(1, 2, ATTR_FLOAT) | ATTRIBUTE(2, 4, ATTR_FLOAT), 3);
        }

        // Calculate VBO points.
        const float x1 = 0;
        const float y1 = 0;
        const float x2 = gpuGetViewportWidth();
        const float y2 = gpuGetViewportHeight();

        // Adjust for power-of-two textures.
        float leftHorizMod = 0;
        float rightHorizMod = (float) (gpuBorderWidth - borderWidth) / (float) gpuBorderWidth;
        float vertMod = (float) (gpuBorderHeight - borderHeight) / (float) gpuBorderHeight;

        if(gameScreen == 1) {
            // Adjust for the bottom screen. (with some error, apparently?)
            static const float mod = ((400.0f - 320.0f) / 400.0f) - (18.0f / 400.0f);
            leftHorizMod = mod / 2.0f;
            rightHorizMod += mod / 2.0f;
        }

        // Prepare new VBO data.
        const float vboData[] = {
                x1, y1, -0.1f, 0.0f + leftHorizMod, 0.0f + vertMod, 1.0f, 1.0f, 1.0f, 1.0f,
                x2, y1, -0.1f, 1.0f - rightHorizMod, 0.0f + vertMod, 1.0f, 1.0f, 1.0f, 1.0f,
                x2, y2, -0.1f, 1.0f - rightHorizMod, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                x2, y2, -0.1f, 1.0f - rightHorizMod, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                x1, y2, -0.1f, 0.0f + leftHorizMod, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                x1, y1, -0.1f, 0.0f + leftHorizMod, 0.0f + vertMod, 1.0f, 1.0f, 1.0f, 1.0f
        };

        // Update the VBO with the new data.
        gpuVboData(borderVbo, vboData, 6 * 9, PRIM_TRIANGLES);
    }

    // Update VBO data if the size has changed.
    if(prevScaleMode != scaleMode || prevScaleFilter != scaleFilter || prevGameScreen != gameScreen) {
        if(prevGameScreen != gameScreen) {
            // Update the viewport.
            gpuViewport(gameScreen == 0 ? TOP_SCREEN : BOTTOM_SCREEN, 0, 0, gameScreen == 0 ? TOP_WIDTH : BOTTOM_WIDTH, gameScreen == 0 ? TOP_HEIGHT : BOTTOM_HEIGHT);
            gputOrtho(0, gameScreen == 0 ? TOP_WIDTH : BOTTOM_WIDTH, 0, gameScreen == 0 ? TOP_HEIGHT : BOTTOM_HEIGHT, -1, 1);
        }

        // Calculate the VBO dimensions.
        u32 vboWidth = 160;
        u32 vboHeight = 144;
        if(scaleMode == 1) {
            vboWidth = 200;
            vboHeight = 180;
        } else if(scaleMode == 2) {
            vboWidth = 240;
            vboHeight = 216;
        } else if(scaleMode == 3) {
            vboWidth *= gpuGetViewportHeight() / (float) 144;
            vboHeight = (u32) gpuGetViewportHeight();
        } else if(scaleMode == 4) {
            vboWidth = (u32) gpuGetViewportWidth();
            vboHeight = (u32) gpuGetViewportHeight();
        }

        // Calculate VBO points.
        const float x1 = (float) ((gpuGetViewportWidth() - vboWidth) / 2);
        const float y1 = (float) ((gpuGetViewportHeight() - vboHeight) / 2);
        const float x2 = x1 + vboWidth;
        const float y2 = y1 + vboHeight;

        // Adjust for power-of-two textures.
        static const float baseHorizMod = (256.0f - 160.0f) / 256.0f;
        static const float baseVertMod = (256.0f - 144.0f) / 256.0f;
        static const float filterMod = 0.25f / 256.0f;

        float horizMod = baseHorizMod;
        float vertMod = baseVertMod;
        if(scaleMode != 0 && scaleFilter == 1) {
            horizMod += filterMod;
            vertMod += filterMod;
        }

        // Prepare new VBO data.
        const float vboData[] = {
                x1, y1, -0.1f, 0.0f, 0.0f + vertMod, 1.0f, 1.0f, 1.0f, 1.0f,
                x2, y1, -0.1f, 1.0f - horizMod, 0.0f + vertMod, 1.0f, 1.0f, 1.0f, 1.0f,
                x2, y2, -0.1f, 1.0f - horizMod, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                x2, y2, -0.1f, 1.0f - horizMod, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                x1, y2, -0.1f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                x1, y1, -0.1f, 0.0f, 0.0f + vertMod, 1.0f, 1.0f, 1.0f, 1.0f
        };

        // Update the VBO with the new data.
        gpuVboData(vbo, vboData, 6 * 9, PRIM_TRIANGLES);

        prevScaleMode = scaleMode;
        prevScaleFilter = scaleFilter;
        prevGameScreen = gameScreen;
    }

    // Update the texture with the new frame.
    TextureFilter filter = scaleFilter >= 1 ? FILTER_LINEAR : FILTER_NEAREST;
    if(scaleMode == 0 || scaleFilter <= 1) {
        gpuTextureData(texture, screenBuffer, 256, 256, PIXEL_RGBA8, TEXTURE_MIN_FILTER(filter) | TEXTURE_MAG_FILTER(filter));
    } else {
        gpuTextureInfo(texture, 512, 512, PIXEL_RGBA8, TEXTURE_MIN_FILTER(filter) | TEXTURE_MAG_FILTER(filter));
        gfxScale2x(screenBuffer);
    }

    // Clear the screen.
    gpuClear();

    // Draw the border.
    if(borderTexture != 0 && borderVbo != 0 && scaleMode != 2) {
        gpuBindTexture(TEXUNIT0, borderTexture);
        gpuDrawVbo(borderVbo);
    }

    // Draw the VBO.
    gpuBindTexture(TEXUNIT0, texture);
    gpuDrawVbo(vbo);

    // Flush GPU commands.
    gpuFlush();

    // Flush GPU framebuffer.
    gpuFlushBuffer();

    if(inputKeyPressed(inputMapFuncKey(FUNC_KEY_SCREENSHOT)) && !isMenuOn()) {
        gputTakeScreenshot();
    }

    // Swap buffers and wait for VBlank.
    gpuSwapBuffers(!gfxGetFastForward());
}

void gfxFlush() {
    gfxFlushBuffers();
}

void gfxWaitForVBlank() {
    gspWaitForVBlank();
}
