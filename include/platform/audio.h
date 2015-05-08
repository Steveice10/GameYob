#pragma once

#include "gb_apu/Blip_Buffer.h"

#include "types.h"

void audioInit();
void audioCleanup();
blip_sample_t* audioGetLeftBuffer();
blip_sample_t* audioGetRightBuffer();
blip_sample_t* audioGetCenterBuffer();
void audioPlay(long leftSamples, long rightSamples, long centerSamples);