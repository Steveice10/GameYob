#include <string.h>

#include "platform/audio.h"
#include "gameboy.h"

#include <ctrcommon/sound.hpp>

static blip_sample_t* audioLeftBuffer = (blip_sample_t*) soundAlloc(APU_BUFFER_SIZE * sizeof(blip_sample_t));
static blip_sample_t* audioRightBuffer = (blip_sample_t*) soundAlloc(APU_BUFFER_SIZE * sizeof(blip_sample_t));
static blip_sample_t* audioCenterBuffer = (blip_sample_t*) soundAlloc(APU_BUFFER_SIZE * sizeof(blip_sample_t));

blip_sample_t* getLeftBuffer() {
    return audioLeftBuffer;
}

blip_sample_t* getRightBuffer() {
    return audioRightBuffer;
}

blip_sample_t* getCenterBuffer() {
    return audioCenterBuffer;
}

void playAudio(long leftSamples, long rightSamples, long centerSamples) {
    soundPlay(0, FORMAT_PCM16, (u32) SAMPLE_RATE, audioLeftBuffer, (u32) leftSamples, 1, -1);
    soundPlay(1, FORMAT_PCM16, (u32) SAMPLE_RATE, audioRightBuffer, (u32) rightSamples, 1, 1);
    soundPlay(2, FORMAT_PCM16, (u32) SAMPLE_RATE, audioCenterBuffer, (u32) centerSamples, 1, 0);
}