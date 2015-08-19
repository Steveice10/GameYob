#ifdef BACKEND_3DS

#include <string.h>

#include "platform/audio.h"
#include "gameboy.h"

#include <citrus/snd.hpp>

using namespace ctr;

static u16* audioLeftBuffer;
static u16* audioRightBuffer;
static u16* audioCenterBuffer;

void audioInit() {
    audioLeftBuffer = (u16*) snd::salloc(APU_BUFFER_SIZE * sizeof(u16));
    audioRightBuffer = (u16*) snd::salloc(APU_BUFFER_SIZE * sizeof(u16));
    audioCenterBuffer = (u16*) snd::salloc(APU_BUFFER_SIZE * sizeof(u16));
}

void audioCleanup() {
    if(audioLeftBuffer != NULL) {
        snd::sfree(audioLeftBuffer);
        audioLeftBuffer = NULL;
    }

    if(audioRightBuffer != NULL) {
        snd::sfree(audioRightBuffer);
        audioRightBuffer = NULL;
    }

    if(audioCenterBuffer != NULL) {
        snd::sfree(audioCenterBuffer);
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
    snd::play(0, audioLeftBuffer, (u32) leftSamples, snd::SAMPLE_PCM16, (u32) SAMPLE_RATE, 1, 0, false);
    snd::play(1, audioRightBuffer, (u32) rightSamples, snd::SAMPLE_PCM16, (u32) SAMPLE_RATE, 0, 1, false);
    snd::play(2, audioCenterBuffer, (u32) centerSamples, snd::SAMPLE_PCM16, (u32) SAMPLE_RATE, 1, 1, false);
    snd::flushCommands();
}

#endif