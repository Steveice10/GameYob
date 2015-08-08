#pragma once

#include "types.h"

void audioInit();
void audioCleanup();
u16* audioGetLeftBuffer();
u16* audioGetRightBuffer();
u16* audioGetCenterBuffer();
void audioPlay(long leftSamples, long rightSamples, long centerSamples);