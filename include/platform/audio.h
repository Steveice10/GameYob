#pragma once

#include "gb_apu/Blip_Buffer.h"

#include "types.h"

blip_sample_t* getLeftBuffer();
blip_sample_t* getRightBuffer();
blip_sample_t* getCenterBuffer();
void playAudio(long leftSamples, long rightSamples, long centerSamples);