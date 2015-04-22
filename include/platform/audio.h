#pragma once

#include "gb_apu/Blip_Buffer.h"

#include <ctrcommon/types.hpp>

blip_sample_t* getAudioBuffer();
void playAudio(long samples);