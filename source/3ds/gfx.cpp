#include "lodepng/lodepng.h"
#include "platform/input.h"
#include "platform/system.h"
#include "ui/config.h"
#include "ui/menu.h"

#include <3ds.h>

#include <ctrcommon/gpu.hpp>

#include "shader_vsh_shbin.h"

static u8* screenBuffer = (u8*) gpuAlloc(256 * 256 * 3);

static int prevScaleMode = -1;
static int prevGameScreen = -1;

static bool fastForward = false;

static u32 shader = 0;
static u32 texture = 0;
static u32 vbo = 0;

static u32 borderVbo = 0;
static u32 borderTexture = 0;

static u32 borderWidth = 0;
static u32 borderHeight = 0;
static u32 gpuBorderWidth = 0;
static u32 gpuBorderHeight = 0;

bool gfxInit() {
    // Initialize the GPU and setup the state.
    if(!gpuInit()) {
        return false;
    }

    gpuCullMode(CULL_BACK_CCW);

    // Load the shader.
    gpuCreateShader(&shader);
    gpuLoadShader(shader, shader_vsh_shbin, shader_vsh_shbin_size);
    gpuUseShader(shader);

    // Create the VBO.
    gpuCreateVbo(&vbo);
    gpuVboAttributes(vbo, ATTRIBUTE(0, 3, ATTR_FLOAT) | ATTRIBUTE(1, 2, ATTR_FLOAT), 2);

    // Create the texture.
    gpuCreateTexture(&texture);

    return true;
}

void gfxCleanup() {
    // Free shader.
    if(shader != 0) {
        gpuFreeShader(shader);
        shader = 0;
    }

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
}

void gfxToggleFastForward() {
    fastForward = !fastForward;
}

void gfxSetFastForward(bool fastforward) {
    fastForward = fastforward;
}

unsigned int gfxNextPowerOfTwo(unsigned int v) {
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}

void gfxLoadBorder(const char* filename) {
    if(filename == NULL) {
        if(borderTexture != 0) {
            gpuFreeVbo(borderTexture);
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

    // Load the image.
    unsigned char* imgData;
    unsigned int imgWidth;
    unsigned int imgHeight;
    if(lodepng_decode32_file(&imgData, &imgWidth, &imgHeight, filename)) {
        return;
    }

    strcpy(borderPath, filename);

    // Adjust the texture to power-of-two dimensions.
    borderWidth = imgWidth;
    borderHeight = imgHeight;
    gpuBorderWidth = gfxNextPowerOfTwo(borderWidth);
    gpuBorderHeight = gfxNextPowerOfTwo(borderHeight);
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
    gpuTextureData(borderTexture, borderBuffer, gpuBorderWidth, gpuBorderHeight, PIXEL_RGBA8, gpuBorderWidth, gpuBorderHeight, PIXEL_RGBA8, TEXTURE_MIN_FILTER(FILTER_LINEAR) | TEXTURE_MAG_FILTER(FILTER_LINEAR));

    // Free the buffer.
    gpuFree(borderBuffer);
}

void gfxDrawPixel(int x, int y, u32 pixel) {
    *(u16*) &screenBuffer[(y * 256 + x) * 3] = (u16) pixel;
    screenBuffer[(y * 256 + x) * 3 + 2] = (u8) (pixel >> 16);
}

void gfxDrawScreen() {
    // Update the border VBO.
    if(borderTexture != 0 && (borderVbo == 0 || prevGameScreen != gameScreen)) {
        // Create the VBO.
        if(borderVbo == 0) {
            gpuCreateVbo(&borderVbo);
            gpuVboAttributes(borderVbo, ATTRIBUTE(0, 3, ATTR_FLOAT) | ATTRIBUTE(1, 2, ATTR_FLOAT), 2);
        }

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

        // Prepare VBO data.
        const float vboData[] = {
                -1, -1, -0.1f, 1.0f - rightHorizMod, 0.0f + vertMod,
                1, -1, -0.1f, 1.0f - rightHorizMod, 1.0f,
                1, 1, -0.1f, 0.0f + leftHorizMod, 1.0f,
                1, 1, -0.1f, 0.0f + leftHorizMod, 1.0f,
                -1, 1, -0.1f, 0.0f + leftHorizMod, 0.0f + vertMod,
                -1, -1, -0.1f, 1.0f - rightHorizMod, 0.0f + vertMod,
        };

        // Update data.
        gpuVboData(borderVbo, vboData, sizeof(vboData), sizeof(vboData) / (5 * 4), PRIM_TRIANGLES);
    }

    // Update VBO data if the size has changed.
    if(prevScaleMode != scaleMode || prevGameScreen != gameScreen) {
        u32 fbWidth = gameScreen == 0 ? 400 : 320;
        u32 fbHeight = 240;

        if(prevGameScreen != gameScreen) {
            // Update the viewport.
            gpuViewport(gameScreen == 0 ? TOP_SCREEN : BOTTOM_SCREEN, 0, 0, fbHeight, fbWidth);
        }

        // Calculate the VBO dimensions.
        u32 vboWidth = 160;
        u32 vboHeight = 144;
        if(scaleMode == 1) {
            vboWidth *= fbHeight / (float) 144;
            vboHeight = fbHeight;
        } else if(scaleMode == 2) {
            vboWidth = fbWidth;
            vboHeight = fbHeight;
        }

        // Calculate VBO extents.
        float horizExtent = vboWidth < fbWidth ? (float) vboWidth / (float) fbWidth : 1;
        float vertExtent = vboHeight < fbHeight ? (float) vboHeight / (float) fbHeight : 1;

        // Adjust for power-of-two textures.
        static float horizMod = (float) (256 - 160) / (float) 256;
        static float vertMod = (float) (256 - 144) / (float) 256;

        // Prepare new VBO data.
        const float vboData[] = {
                -vertExtent, -horizExtent, -0.1f, 1.0f - horizMod, 0.0f + vertMod,
                vertExtent, -horizExtent, -0.1f, 1.0f - horizMod, 1.0f,
                vertExtent, horizExtent, -0.1f, 0.0f, 1.0f,
                vertExtent, horizExtent, -0.1f, 0.0f, 1.0f,
                -vertExtent, horizExtent, -0.1f, 0.0f, 0.0f + vertMod,
                -vertExtent, -horizExtent, -0.1f, 1.0f - horizMod, 0.0f + vertMod,
        };

        // Update the VBO with the new data.
        gpuVboData(vbo, vboData, sizeof(vboData), sizeof(vboData) / (5 * 4), PRIM_TRIANGLES);

        prevScaleMode = scaleMode;
        prevGameScreen = gameScreen;
    }

    // Update the texture with the new frame.
    TextureFilter filter = scaleFilter == 1 ? FILTER_LINEAR : FILTER_NEAREST;
    gpuTextureData(texture, screenBuffer, 256, 256, PIXEL_RGB8, 256, 256, PIXEL_RGB8, TEXTURE_MIN_FILTER(filter) | TEXTURE_MAG_FILTER(filter));

    // Clear the screen.
    gpuClear();

    // Draw the border.
    if(borderTexture != 0 && borderVbo != 0 && customBordersEnabled && scaleMode != 2) {
        gpuBindTexture(TEXUNIT0, borderTexture);
        gpuDrawVbo(borderVbo);
    }

    // Draw the VBO.
    gpuBindTexture(TEXUNIT0, texture);
    gpuDrawVbo(vbo);

    // Flush GPU commands.
    gpuFlush();

    // Swap buffers and wait for VBlank.
    gpuSwapBuffers(!(fastForward || inputKeyHeld(mapFuncKey(FUNC_KEY_FAST_FORWARD))));
}

void gfxFlush() {
    gfxFlushBuffers();
}

void gfxWaitForVBlank() {
    gspWaitForVBlank();
}

void gfxClearScreens() {
    screenClearAll(0, 0, 0);
}
