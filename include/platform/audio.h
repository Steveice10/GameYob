#pragma once

#include "gb_apu/Blip_Buffer.h"

#include "types.h"

blip_sample_t* getAudioBuffer();
void playAudio(long samples);