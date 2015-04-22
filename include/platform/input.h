#pragma once

#include "types.h"

#define NUM_BINDABLE_BUTTONS 32

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
int inputMapFuncKey(int funcKey);
int inputMapMenuKey(int menuKey);
