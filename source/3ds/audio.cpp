#include <string.h>

#include "platform/audio.h"
#include "gameboy.h"

#include <ctrcommon/sound.hpp>

static blip_sample_t* audioLeftBuffer;
static blip_sample_t* audioRightBuffer;
static blip_sample_t* audioCenterBuffer;

void audioInit() {
    audioLeftBuffer = (blip_sample_t*) soundAlloc(APU_BUFFER_SIZE * sizeof(blip_sample_t));
    audioRightBuffer = (blip_sample_t*) soundAlloc(APU_BUFFER_SIZE * sizeof(blip_sample_t));
    audioCenterBuffer = (blip_sample_t*) soundAlloc(APU_BUFFER_SIZE * sizeof(blip_sample_t));
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

blip_sample_t* audioGetLeftBuffer() {
    return audioLeftBuffer;
}

blip_sample_t* audioGetRightBuffer() {
    return audioRightBuffer;
}

blip_sample_t* audioGetCenterBuffer() {
    return audioCenterBuffer;
}

void audioPlay(long leftSamples, long rightSamples, long centerSamples) {
    soundSetChannel(0, audioLeftBuffer, (u32) leftSamples, SAMPLE_PCM16, (u32) SAMPLE_RATE, 1, 0, false);
    soundSetChannel(1, audioRightBuffer, (u32) rightSamples, SAMPLE_PCM16, (u32) SAMPLE_RATE, 0, 1, false);
    soundSetChannel(2, audioCenterBuffer, (u32) centerSamples, SAMPLE_PCM16, (u32) SAMPLE_RATE, 1, 1, false);
    soundFlush();
}