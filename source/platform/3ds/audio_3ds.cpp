#ifdef BACKEND_3DS

#include <string.h>

#include "platform/audio.h"
#include "gameboy.h"

#include <ctrcommon/sound.hpp>

static u16* audioLeftBuffer;
static u16* audioRightBuffer;
static u16* audioCenterBuffer;

void audioInit() {
    audioLeftBuffer = (u16*) soundAlloc(APU_BUFFER_SIZE * sizeof(u16));
    audioRightBuffer = (u16*) soundAlloc(APU_BUFFER_SIZE * sizeof(u16));
    audioCenterBuffer = (u16*) soundAlloc(APU_BUFFER_SIZE * sizeof(u16));
}

void audioCleanup() {
    if(audioLeftBuffer != NULL) {
        soundFree(audioLeftBuffer);
        audioLeftBuffer = NULL;
    }

    if(audioRightBuffer != NULL) {
        soundFree(audioRightBuffer);
        audioRightBuffer = NULL;
    }

    if(audioCenterBuffer != NULL) {
        soundFree(audioCenterBuffer);
        audioCenterBuffer = NULL;
    }
}

u16* audioGetLeftBuffer() {
    return audioLeftBuffer;
}

u16* audioGetRightBuffer() {
    return audioRightBuffer;
}

u16* audioGetCenterBuffer() {
    return audioCenterBuffer;
}

void audioPlay(long leftSamples, long rightSamples, long centerSamples) {
    soundPlay(0, audioLeftBuffer, (u32) leftSamples, SAMPLE_PCM16, (u32) SAMPLE_RATE, 1, 0, false);
    soundPlay(1, audioRightBuffer, (u32) rightSamples, SAMPLE_PCM16, (u32) SAMPLE_RATE, 0, 1, false);
    soundPlay(2, audioCenterBuffer, (u32) centerSamples, SAMPLE_PCM16, (u32) SAMPLE_RATE, 1, 1, false);
    soundFlush();
}

#endif