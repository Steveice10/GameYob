#include "gbgfx.h"
#include "inputhelper.h"

#include <3ds.h>

#include <ctrcommon/gpu.hpp>

#include "shader_vsh_shbin.h"

int prevScaleMode = -1;
int prevGameScreen = -1;

bool fastForward = false;

u32 shader = 0;
u32 texture = 0;
u32 vbo = 0;

void gfxInit() {
    // Initialize the GPU and setup the state.
    gpuInit();
    gpuCullMode(CULL_BACK_CCW);

    // Load the shader.
    gpuCreateShader(&shader);
    gpuLoadShader(shader, shader_vsh_shbin, shader_vsh_shbin_size);
    gpuUseShader(shader);

    // Create and bind the texture.
    gpuCreateTexture(&texture);
    gpuBindTexture(TEXUNIT0, texture);

    // Create the VBO.
    gpuCreateVbo(&vbo);
    gpuVboAttributes(vbo, ATTRIBUTE(0, 3, ATTR_FLOAT) | ATTRIBUTE(1, 2, ATTR_FLOAT), 2);
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
}

void gfxToggleFastForward() {
    fastForward = !fastForward;
}

void gfxSetFastForward(bool fastforward) {
    fastForward = fastforward;
}

void gfxDrawScreen(u8* screenBuffer, int scaleMode, int gameScreen) {
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
            float scale = fbHeight / (float) 144;
            vboWidth *= scale;
            vboHeight *= scale;
        } else if(scaleMode == 2) {
            vboWidth *= fbWidth / (float) 160;
            vboHeight *= fbHeight / (float) 144;
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
    gpuTextureData(texture, screenBuffer, 256, 256, PIXEL_RGB8, 256, 256, PIXEL_RGB8,
                   TEXTURE_MIN_FILTER(filter) | TEXTURE_MAG_FILTER(filter));

    // Draw the VBO.
    gpuClear();
    gpuDrawVbo(vbo);
    gpuFlush();

    // Swap buffers and wait for VBlank.
    gpuSwapBuffers(!(fastForward || keyPressed(mapFuncKey(FUNC_KEY_FAST_FORWARD))));
}

void gfxFlush() {
    gfxFlushBuffers();
}

void gfxWaitForVBlank() {
    gspWaitForVBlank();
}
