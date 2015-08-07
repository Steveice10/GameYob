#pragma once

#include "types.h"

#ifdef BACKEND_3DS
    #define NUM_BUTTONS 32
#elif defined(BACKEND_SDL)
    #define NUM_BUTTONS 512
#else
    #define NUM_BUTTONS 0
#endif

void inputInit();
void inputCleanup();
void inputUpdate();
const char* inputGetKeyName(int keyIndex);
bool inputIsValidKey(int keyIndex);
bool inputKeyHeld(int key);
bool inputKeyRepeat(int key);
bool inputKeyPressed(int key);
void inputKeyRelease(int key);
int inputGetMotionSensorX();
int inputGetMotionSensorY();

struct KeyConfig;

KeyConfig inputGetDefaultKeyConfig();
void inputLoadKeyConfig(KeyConfig* keyConfig);
