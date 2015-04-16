#pragma once

#include <stdio.h>
#include "config.h"

#include <ctrcommon/types.hpp>

#define FAT_CACHE_SIZE 16

#define GB_A            0x01
#define GB_B            0x02
#define GB_SELECT        0x04
#define GB_START        0x08
#define GB_RIGHT        0x10
#define GB_LEFT            0x20
#define GB_UP            0x40
#define GB_DOWN            0x80

extern char borderPath[256];

void flushFatCache();

bool keyPressed(int key);
bool keyPressedAutoRepeat(int key);
bool keyJustPressed(int key);
// Consider this key unpressed until released and pressed again
void forceReleaseKey(int key);

void inputUpdate();

int inputGetMotionSensorX();
int inputGetMotionSensorY();

void systemCheckPolls();
void systemCleanup();
