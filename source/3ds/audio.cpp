#include <string.h>

#include "platform/audio.h"
#include "gameboy.h"

#include <ctrcommon/sound.hpp>

static blip_sample_t* audioBuffer = (blip_sample_t*) soundAlloc(APU_BUFFER_SIZE * sizeof(blip_sample_t));

blip_sample_t* getAudioBuffer() {
    return audioBuffer;
}

void playAudio(long samples) {
    soundPlay(0, FORMAT_PCM16, (u32) SAMPLE_RATE, audioBuffer, (u32) samples);
}