#include <string.h>

#include "audio.h"
#include "gameboy.h"
#include "soundengine.h"

#include <ctrcommon/sound.hpp>

#define FRAMES_PER_BUFFER 8

#define CSND_FREQUENCY (CYCLES_PER_FRAME * 59.7 / CYCLES_UNTIL_SAMPLE)
#define CSND_BUFFER_SIZE ((CYCLES_PER_FRAME / CYCLES_UNTIL_SAMPLE) * FRAMES_PER_BUFFER)

s16* playBuffer = (s16*) soundAlloc(2 * CSND_BUFFER_SIZE);
s16* recordBuffer = (s16*) soundAlloc(2 * CSND_BUFFER_SIZE);

int recordingPos = 0;

void initSampler() {
    recordingPos = 0;
}

void swapBuffers() {
    recordingPos = 0;
    memcpy(playBuffer, recordBuffer, CSND_BUFFER_SIZE * 2);
    soundPlay(0, FORMAT_PCM16, (u32) CSND_FREQUENCY, playBuffer, CSND_BUFFER_SIZE);
}

// Called once every 4 cycles
void addSample(s16 sample) {
    if(recordingPos >= CSND_BUFFER_SIZE) {
        swapBuffers();
        return;
    }

    recordBuffer[recordingPos++] = sample;
}