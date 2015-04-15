#include <3ds.h>
#include <inputhelper.h>
#include "3ds/3dsgfx.h"

#include <ctrcommon/gpu.hpp>

#include "shader_vsh_shbin.h"

int prevScaleMode = -1;
int prevGameScreen = -1;

u32 shader = 0;
u32 texture = 0;
u32 vbo = 0;

void initGPU() {
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

    // Initialize the screen to black.
    gpuClear();
}

void deinitGPU() {
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

void drawGPU(u8* screenBuffer, int scaleMode, int gameScreen) {
    // Update VBO data if the size has changed.
    if(prevScaleMode != scaleMode || prevGameScreen != gameScreen) {
        int fbWidth = gameScreen == 0 ? TOP_SCREEN_WIDTH : BOTTOM_SCREEN_WIDTH;
        int fbHeight = gameScreen == 0 ? TOP_SCREEN_HEIGHT : BOTTOM_SCREEN_HEIGHT;

        if(prevGameScreen != gameScreen) {
            // Update the viewport.
            gpuViewport(gameScreen == 0 ? TOP_SCREEN : BOTTOM_SCREEN, 0, 0, (u32) fbHeight, (u32) fbWidth);
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
    gpuTextureData(texture, screenBuffer, 256, 256, PIXEL_RGB8, 256, 256, PIXEL_RGB8, TEXTURE_MIN_FILTER(FILTER_LINEAR) | TEXTURE_MAG_FILTER(FILTER_LINEAR));

    // Draw the VBO.
    gpuClear();
    gpuDrawVbo(vbo);
    gpuFlush();

    // Swap buffers and wait for VBlank.
    system_waitForVBlank();
}

u8 gfxBuffer = 0;

void drawPixel(u8* framebuffer, int x, int y, u32 color) {
    y = TOP_SCREEN_HEIGHT - y - 1;

    u8* ptr = framebuffer + (x*TOP_SCREEN_HEIGHT + y)*3;

    *(u16*)ptr = color;
    *(ptr+2) = color>>16;
}

u8* gfxGetActiveFramebuffer(gfxScreen_t screen, gfx3dSide_t side) {
    u8** buf;
    if (screen == GFX_TOP)
        buf = gfxTopLeftFramebuffers;
    else
        buf = gfxBottomFramebuffers;
    return buf[gfxBuffer];
}

u8* gfxGetInactiveFramebuffer(gfxScreen_t screen, gfx3dSide_t side) {
    u8** buf;
    if (screen == GFX_TOP)
        buf = gfxTopLeftFramebuffers;
    else
        buf = gfxBottomFramebuffers;
    return buf[!gfxBuffer];
}

void gfxMySwapBuffers() {
    gfxBuffer ^= 1;
    gpuSwapBuffers(!(fastForwardMode || fastForwardKey));
}
