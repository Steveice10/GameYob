#pragma once

#include <ctrcommon/types.hpp>

void inputUpdate();
bool inputKeyHeld(int key);
bool inputKeyRepeat(int key);
bool inputKeyPressed(int key);
void inputKeyRelease(int key);
int inputGetMotionSensorX();
int inputGetMotionSensorY();
