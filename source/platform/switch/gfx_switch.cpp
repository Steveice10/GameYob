#ifdef BACKEND_SWITCH

#include <string.h>
#include <time.h>

#include <fstream>
#include <sstream>

#include <switch.h>

#include "platform/common/menu/menu.h"
#include "platform/gfx.h"
#include "platform/system.h"

#define SCREEN_BUFFER_WIDTH 256
#define SCREEN_BUFFER_HEIGHT 224
#define SCREEN_BUFFER_PIXELS (SCREEN_BUFFER_WIDTH * SCREEN_BUFFER_HEIGHT)
#define SCREEN_BUFFER_SIZE (SCREEN_BUFFER_PIXELS * sizeof(u32))

#define SCALE2X_BUFFER_WIDTH 512
#define SCALE2X_BUFFER_HEIGHT 448
#define SCALE2X_BUFFER_PIXELS (SCALE2X_BUFFER_WIDTH * SCALE2X_BUFFER_HEIGHT)
#define SCALE2X_BUFFER_SIZE (SCALE2X_BUFFER_PIXELS * sizeof(u32))

static u32* borderBuffer;
static u32 borderWidth;
static u32 borderHeight;

static u32* screenBuffer;
static u32* scale2xBuffer;

bool gfxInit() {
    gfxInitDefault();

    borderBuffer = NULL;

    screenBuffer = (u32*) calloc(SCREEN_BUFFER_PIXELS, sizeof(u32));
    if(screenBuffer == NULL) {
        return false;
    }

    memset(screenBuffer, 0, SCREEN_BUFFER_SIZE);

    scale2xBuffer = (u32*) calloc(SCALE2X_BUFFER_PIXELS, sizeof(u32));
    if(scale2xBuffer == NULL) {
        free(screenBuffer);
        screenBuffer = NULL;

        return false;
    }

    memset(scale2xBuffer, 0, SCALE2X_BUFFER_SIZE);

    return true;
}

void gfxCleanup() {
    if(scale2xBuffer != NULL) {
        free(scale2xBuffer);
        scale2xBuffer = NULL;
    }

    if(screenBuffer != NULL) {
        free(screenBuffer);
        screenBuffer = NULL;
    }

    gfxExit();
}

void gfxLoadBorder(u8* imgData, int imgWidth, int imgHeight) {
    if(imgData == NULL || (borderBuffer != NULL && (borderWidth != (u32) imgWidth || borderHeight != (u32) imgHeight))) {
        if(borderBuffer != NULL) {
            free(borderBuffer);
            borderBuffer = NULL;
        }

        borderWidth = 0;
        borderHeight = 0;

        if(imgData == NULL) {
            return;
        }
    }

    // Create the texture buffer.
    if(borderBuffer == NULL) {
        borderBuffer = (u32*) malloc(imgWidth * imgHeight * sizeof(u32));
        if(borderBuffer == NULL) {
            return;
        }
    }

    borderWidth = (u32) imgWidth;
    borderHeight = (u32) imgHeight;

    memcpy(borderBuffer, imgData, borderWidth * borderHeight * sizeof(u32));
}

u32* gfxGetScreenBuffer() {
    return screenBuffer;
}

u32 gfxGetScreenPitch() {
    return SCREEN_BUFFER_WIDTH;
}

void gfxTakeScreenshot() {
    u32 width = 0;
    u32 height = 0;
    u8* fb = gfxGetFramebuffer(&height, &width);

    if(fb == NULL) {
        return;
    }

    u32 headerSize = 0x36;
    u32 imageSize = width * height * 3;

    u8* header = new u8[headerSize]();

    *(u16*) &header[0x0] = 0x4D42;
    *(u32*) &header[0x2] = headerSize + imageSize;
    *(u32*) &header[0xA] = headerSize;
    *(u32*) &header[0xE] = 0x28;
    *(u32*) &header[0x12] = width;
    *(u32*) &header[0x16] = height;
    *(u32*) &header[0x1A] = 0x00180001;
    *(u32*) &header[0x22] = imageSize;

    u8* image = new u8[imageSize]();

    for(u32 x = 0; x < width; x++) {
        for(u32 y = 0; y < height; y++) {
            u8* src = &fb[(x * height + y) * 3];
            u8* dst = &image[(y * width + x) * 3];

            *(u16*) dst = *(u16*) src;
            dst[2] = src[2];
        }
    }

    std::stringstream fileStream;
    fileStream << "/gameyob_" << time(NULL) << ".bmp";

    std::ofstream stream(fileStream.str(), std::ios::binary);
    if(stream.is_open()) {
        stream.write((char*) header, (size_t) headerSize);
        stream.write((char*) image, (size_t) imageSize);
        stream.close();
    } else {
        systemPrintDebug("Failed to open screenshot file: %s\n", strerror(errno));
    }

    delete[] header;
    delete[] image;
}

#define SEEK_PIXEL(buf, x, y, pitch) ((buf) + ((((y) * (pitch)) + (x))))
#define DO_SCALE2X() if(Bp != Hp && Dp != Fp) {                    \
                         if(Dp == Bp) *(E0) = Dp;                  \
                         else *(E0) = Ep;                          \
                         if(Bp == Fp) *((E0) + 1) = Fp;            \
                         else *((E0) + 1) = Ep;                    \
                         if(Dp == Hp) *((E0) + dstPitch) = Dp;     \
                         else *((E0) + dstPitch) = Ep;             \
                         if(Hp == Fp) *((E0) + dstPitch + 1) = Fp; \
                         else *((E0) + dstPitch + 1) = Ep;         \
                     } else {                                      \
                         *(E0) = Ep;                               \
                         *((E0) + 1) = Ep;                         \
                         *((E0) + dstPitch) = Ep;                  \
                         *((E0) + dstPitch + 1) = Ep;              \
                     }

void gfxScale2xRGBA8888(u32* src, u32 srcPitch, u32* dst, u32 dstPitch, int width, int height) {
    u32* E, *E0;
    u32 Ep, Bp, Dp, Fp, Hp;

    // Top line and top corners
    E = SEEK_PIXEL(src, 0, 0, srcPitch);
    E0 = SEEK_PIXEL(dst, 0, 0, dstPitch);
    Ep = E[0];
    Bp = Ep;
    Dp = Ep;
    Fp = E[1];
    Hp = E[srcPitch];
    DO_SCALE2X();

    for(int x = 1; x < width - 1; x++) {
        E += 1;
        E0 += 2;
        Dp = Ep;
        Ep = Fp;
        Fp = E[1];
        Bp = Ep;
        Hp = E[srcPitch];
        DO_SCALE2X();
    }

    E += 1;
    E0 += 2;
    Dp = Ep;
    Ep = Fp;
    Bp = Ep;
    Hp = E[srcPitch];
    DO_SCALE2X();

    // Middle Rows and sides
    for(int y = 1; y < height - 1; y++) {
        E = SEEK_PIXEL(src, 0, y, srcPitch);
        E0 = SEEK_PIXEL(dst, 0, y * 2, dstPitch);
        Ep = E[0];
        Bp = E[-srcPitch];
        Dp = Ep;
        Fp = E[1];
        Hp = E[srcPitch];
        DO_SCALE2X();

        for(int x = 1; x < width - 1; x++) {
            E += 1;
            E0 += 2;
            Dp = Ep;
            Ep = Fp;
            Fp = E[1];
            Bp = E[-srcPitch];
            Hp = E[srcPitch];
            DO_SCALE2X();
        }

        E += 1;
        E0 += 2;
        Dp = Ep;
        Ep = Fp;
        Bp = E[-srcPitch];
        Hp = E[srcPitch];
        DO_SCALE2X();
    }

    // Bottom Row and Bottom Corners
    E = SEEK_PIXEL(src, 0, height - 1, srcPitch);
    E0 = SEEK_PIXEL(dst, 0, (height - 1) * 2, dstPitch);
    Ep = E[0];
    Bp = E[-srcPitch];
    Dp = Ep;
    Fp = E[1];
    Hp = Ep;
    DO_SCALE2X();

    for(int x = 1; x < width - 1; x++) {
        E += 1;
        E0 += 2;
        Dp = Ep;
        Ep = Fp;
        Fp = E[1];
        Bp = E[-srcPitch];
        Hp = Ep;
        DO_SCALE2X();
    }

    E += 1;
    E0 += 2;
    Dp = Ep;
    Ep = Fp;
    Bp = E[-srcPitch];
    Hp = Ep;
    DO_SCALE2X();
}

void gfxDrawScreen() {
    u32 screenTexWidth = SCREEN_BUFFER_WIDTH;
    u32 screenTexHeight = SCREEN_BUFFER_HEIGHT;
    u32* transferBuffer = screenBuffer;

    if(scaleMode != 0 && scaleFilter == 2) {
        screenTexWidth = SCALE2X_BUFFER_WIDTH;
        screenTexHeight = SCALE2X_BUFFER_HEIGHT;
        transferBuffer = scale2xBuffer;

        gfxScale2xRGBA8888(screenBuffer, SCREEN_BUFFER_WIDTH, scale2xBuffer, SCALE2X_BUFFER_WIDTH, SCREEN_BUFFER_WIDTH, SCREEN_BUFFER_HEIGHT);
    }

    u32 viewportWidth = 0;
    u32 viewportHeight = 0;
    u32* framebuffer = (u32*) gfxGetFramebuffer(&viewportWidth, &viewportHeight);

    // Draw the border.
    if(borderBuffer != NULL && scaleMode != 4) {
        // Calculate output dimensions.
        u32 scaledBorderWidth = borderWidth;
        u32 scaledBorderHeight = borderHeight;

        if(borderScaleMode == 1) {
            if(scaleMode == 1) {
                scaledBorderWidth *= 1.25f;
                scaledBorderHeight *= 1.25f;
            } else if(scaleMode == 2) {
                scaledBorderWidth *= 1.50f;
                scaledBorderHeight *= 1.50f;
            } else if(scaleMode == 3) {
                scaledBorderWidth *= viewportHeight / (float) SCREEN_BUFFER_HEIGHT;
                scaledBorderHeight *= viewportHeight / (float) SCREEN_BUFFER_HEIGHT;
            } else if(scaleMode == 4) {
                scaledBorderWidth *= viewportWidth / (float) SCREEN_BUFFER_WIDTH;
                scaledBorderHeight *= viewportHeight / (float) SCREEN_BUFFER_HEIGHT;
            }
        }

        // Calculate output points.
        const u32 x1 = ((int) viewportWidth - scaledBorderWidth) / 2;
        const u32 y1 = ((int) viewportHeight - scaledBorderHeight) / 2;

        for(u32 x = 0; x < scaledBorderWidth; x++) {
            for(u32 y = 0; y < scaledBorderHeight; y++) {
                framebuffer[gfxGetFramebufferDisplayOffset(x + x1, y + y1)] = __builtin_bswap32(borderBuffer[(y * borderHeight / scaledBorderHeight) * borderWidth + (x * borderWidth / scaledBorderWidth)]);
            }
        }
    }

    // Draw the screen.
    {
        // Calculate output dimensions.
        u32 screenWidth = SCREEN_BUFFER_WIDTH;
        u32 screenHeight = SCREEN_BUFFER_HEIGHT;

        if(scaleMode == 1) {
            screenWidth *= 1.25f;
            screenHeight *= 1.25f;
        } else if(scaleMode == 2) {
            screenWidth *= 1.50f;
            screenHeight *= 1.50f;
        } else if(scaleMode == 3) {
            screenWidth *= viewportHeight / (float) screenHeight;
            screenHeight = viewportHeight;
        } else if(scaleMode == 4) {
            screenWidth = viewportWidth;
            screenHeight = viewportHeight;
        }

        // Calculate output points.
        const u32 x1 = ((int) viewportWidth - screenWidth) / 2;
        const u32 y1 = ((int) viewportHeight - screenHeight) / 2;

        for(u32 x = 0; x < screenWidth; x++) {
            for(u32 y = 0; y < screenHeight; y++) {
                framebuffer[gfxGetFramebufferDisplayOffset(x + x1, y + y1)] = __builtin_bswap32(transferBuffer[(y * screenTexHeight / screenHeight) * screenTexWidth + (x * screenTexWidth / screenWidth)]);
            }
        }
    }

    gfxFlushBuffers();
    gfxSwapBuffers();
    gfxWaitForVsync();
}

#endif
