#pragma once

#include "types.h"

bool audioInit();
void audioCleanup();
u16 audioGetSampleRate();
void audioClear();
void audioPlay(u32* buffer, long samples);