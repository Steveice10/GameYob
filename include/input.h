#pragma once

#include <ctrcommon/types.hpp>

#define GB_A            0x01
#define GB_B            0x02
#define GB_SELECT        0x04
#define GB_START        0x08
#define GB_RIGHT        0x10
#define GB_LEFT            0x20
#define GB_UP            0x40
#define GB_DOWN            0x80

void inputUpdate();
bool inputKeyHeld(int key);
bool inputKeyRepeat(int key);
bool inputKeyPressed(int key);
void inputKeyRelease(int key);
int inputGetMotionSensorX();
int inputGetMotionSensorY();
