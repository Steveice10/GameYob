#pragma once

#include "types.h"

enum {
    FUNC_KEY_NONE = 0,
    FUNC_KEY_A = 1,
    FUNC_KEY_B = 2,
    FUNC_KEY_LEFT = 3,
    FUNC_KEY_RIGHT = 4,
    FUNC_KEY_UP = 5,
    FUNC_KEY_DOWN = 6,
    FUNC_KEY_START = 7,
    FUNC_KEY_SELECT = 8,
    FUNC_KEY_MENU = 9,
    FUNC_KEY_MENU_PAUSE = 10,
    FUNC_KEY_SAVE = 11,
    FUNC_KEY_AUTO_A = 12,
    FUNC_KEY_AUTO_B = 13,
    FUNC_KEY_FAST_FORWARD = 14,
    FUNC_KEY_FAST_FORWARD_TOGGLE = 15,
    FUNC_KEY_SCALE = 16,
    FUNC_KEY_RESET = 17,
    FUNC_KEY_SCREENSHOT = 18,

    NUM_FUNC_KEYS = 19
};

void inputInit();
void inputCleanup();
void inputUpdate();
const char* inputGetKeyName(int keyIndex);
bool inputIsValidKey(int keyIndex);
bool inputKeyHeld(int key);
bool inputKeyRepeat(int key);
bool inputKeyPressed(int key);
void inputKeyRelease(int key);
void inputReleaseAll();
int inputGetMotionSensorX();
int inputGetMotionSensorY();

struct KeyConfig;

KeyConfig inputGetDefaultKeyConfig();
void inputLoadKeyConfig(KeyConfig* keyConfig);
