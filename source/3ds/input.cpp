#include <stdlib.h>

#include "gfx.h"
#include "input.h"
#include "menu.h"

#include <3ds.h>

#include <ctrcommon/input.hpp>
#include <ctrcommon/platform.hpp>
#include <ctrcommon/screen.hpp>

u32 keysForceReleased = 0;
u64 nextRepeat = 0;

void inputUpdate() {
    inputPoll();
    for(int i = 0; i < 32; i++) {
        if(!inputIsPressed((Button) (1 << i))) {
            keysForceReleased &= ~(1 << i);
        }
    }

    if(accelPadMode && inputIsPressed(BUTTON_TOUCH) && inputGetTouch().x <= screenGetStrWidth("Exit") && inputGetTouch().y <= screenGetStrHeight("Exit")) {
        accelPadMode = false;
        consoleClear();
    }
}

bool inputKeyHeld(int key) {
    return inputIsHeld((Button) key) && !(keysForceReleased & key);
}

bool inputKeyPressed(int key) {
    return inputIsPressed((Button) key) && !(keysForceReleased & key);
}

bool inputKeyRepeat(int key) {
    if(inputKeyPressed(key)) {
        nextRepeat = platformGetTime() + 250;
        return true;
    }

    if(inputKeyHeld(key) && platformGetTime() >= nextRepeat) {
        nextRepeat = platformGetTime() + 50;
        return true;
    }

    return false;
}

void inputKeyRelease(int key) {
    keysForceReleased |= key;
}

int inputGetMotionSensorX() {
    int accelX = accelPadMode ? (inputIsHeld(BUTTON_TOUCH) ? 160 - inputGetTouch().x : 0) : 0;
    return 2047 + accelX;
}

int inputGetMotionSensorY() {
    int accelY = accelPadMode ? (inputIsHeld(BUTTON_TOUCH) ? 120 - inputGetTouch().y : 0) : 0;
    return 2047 + accelY;
}
