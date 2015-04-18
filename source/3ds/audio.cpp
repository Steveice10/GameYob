#include <string.h>

#include "platform/audio.h"
#include "gameboy.h"

#include <ctrcommon/sound.hpp>

s16* playBuffer = (s16*) soundAlloc(2 * APU_BUFFER_SIZE);

void playAudio(s16* samples, long count) {
    memcpy(playBuffer, samples, (u32) count * 2);
    soundPlay(0, FORMAT_PCM16, (u32) SAMPLE_RATE, playBuffer, (u32) count);
}