#include <stdlib.h>

#include "inputhelper.h"
#include "console.h"
#include "gbmanager.h"
#include "gfx.h"

#include <ctrcommon/input.hpp>
#include <ctrcommon/platform.hpp>

u32 keysForceReleased = 0;
u64 nextRepeat = 0;

void flushFatCache() {
}

bool keyPressed(int key) {
    return inputIsHeld((Button) key) && !(keysForceReleased & key);
}

bool keyJustPressed(int key) {
    return inputIsPressed((Button) key) && !(keysForceReleased & key);
}

bool keyPressedAutoRepeat(int key) {
    if(keyJustPressed(key)) {
        nextRepeat = platformGetTime() + 250;
        return true;
    }

    if(keyPressed(key) && platformGetTime() >= nextRepeat) {
        nextRepeat = platformGetTime() + 50;
        return true;
    }

    return false;
}

void forceReleaseKey(int key) {
    keysForceReleased |= key;
}

void inputUpdate() {
    inputPoll();
    for(int i = 0; i < 32; i++) {
        if(!inputIsPressed((Button) (1 << i))) {
            keysForceReleased &= ~(1 << i);
        }
    }
}

int inputGetMotionSensorX() {
    int px = keyPressed(BUTTON_TOUCH) ? inputGetTouch().x : 128;
    double val = (128 - px) * ((double) MOTION_SENSOR_RANGE / 256) + MOTION_SENSOR_MID;
    return (int) val + 0x8000;
}

int inputGetMotionSensorY() {
    int py = keyPressed(BUTTON_TOUCH) ? inputGetTouch().y : 96;
    double val = (96 - py) * ((double) MOTION_SENSOR_RANGE / 192) + MOTION_SENSOR_MID - 80;
    return (int) val;
}

void systemCheckPolls() {
    if(!platformIsRunning()) {
        systemCleanup();
        exit(0);
    }
}

void systemCleanup() {
    mgr_save();
    mgr_exit();

    consoleCleanup();
    gfxCleanup();
    platformCleanup();
}
