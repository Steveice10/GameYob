#ifdef BACKEND_3DS

#include <sys/unistd.h>
#include <string.h>

#include "platform/audio.h"
#include "platform/gfx.h"

#include <3ds.h>

#define BUFFER_SAMPLES 2048
#define NUM_BUFFERS 4

static ndspWaveBuf waveBuf[NUM_BUFFERS];
static u32* audioBuffer;

static u32 currBuffer;
static u32 currPos;

bool audioInit() {
    if(R_FAILED(ndspInit())) {
        return false;
    }

    u32 bufSize = BUFFER_SAMPLES * NUM_BUFFERS * sizeof(u32);
    audioBuffer = (u32*) linearAlloc(bufSize);
    if(audioBuffer == NULL) {
        audioCleanup();
        return false;
    }

    memset(audioBuffer, 0, bufSize);

    currBuffer = 0;
    currPos = 0;

    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, audioGetSampleRate());
    ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);

    float mix[12] = {1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    ndspChnSetMix(0, mix);

    memset(waveBuf, 0, sizeof(waveBuf));
    for(int i = 0; i < NUM_BUFFERS; i++) {
        waveBuf[i].data_vaddr = &audioBuffer[BUFFER_SAMPLES * i];
        waveBuf[i].nsamples = BUFFER_SAMPLES;
        ndspChnWaveBufAdd(0, &waveBuf[i]);
    }

    return true;
}

void audioCleanup() {
    ndspExit();

    if(audioBuffer != NULL) {
        linearFree(audioBuffer);
        audioBuffer = NULL;
    }
}

u16 audioGetSampleRate() {
    return 44100;
}

void audioClear() {
    ndspChnWaveBufClear(0);
    for(int i = 0; i < NUM_BUFFERS; i++) {
        waveBuf[i].status = NDSP_WBUF_FREE;
    }
}

void audioPlay(u32* buffer, long samples) {
    long remaining = samples;
    while(remaining > 0) {
        ndspWaveBuf* buf = &waveBuf[currBuffer];

        if(buf->status != NDSP_WBUF_DONE && buf->status != NDSP_WBUF_FREE) {
            if(gfxGetFastForward()) {
                audioClear();
                ndspChnWaveBufAdd(0, buf->next);
            } else {
                usleep(10);
                continue;
            }
        }

        long currSamples = remaining;
        if((u32) currSamples > buf->nsamples - currPos) {
            currSamples = buf->nsamples - currPos;
        }

        memcpy(&((u32*) buf->data_vaddr)[currPos], &buffer[samples - remaining], (size_t) currSamples * sizeof(u32));

        currPos += currSamples;
        remaining -= currSamples;

        if(currPos >= buf->nsamples) {
            DSP_FlushDataCache(buf->data_vaddr, buf->nsamples * sizeof(u32));
            ndspChnWaveBufAdd(0, buf);

            currPos -= buf->nsamples;
            currBuffer = (currBuffer + 1) % NUM_BUFFERS;
        }
    }
}

#endif