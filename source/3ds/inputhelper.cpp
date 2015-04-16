#include <stdlib.h>

#include "console.h"
#include "gbmanager.h"
#include "gfx.h"
#include "inputhelper.h"
#include "menu.h"

#include <3ds.h>

#include <ctrcommon/input.hpp>
#include <ctrcommon/platform.hpp>
#include <ctrcommon/screen.hpp>

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

    if(accelPadMode && inputIsPressed(BUTTON_TOUCH) && inputGetTouch().x <= screenGetStrWidth("Exit") && inputGetTouch().y <= screenGetStrHeight("Exit")) {
        accelPadMode = false;
        consoleClear();
    }
}

int inputGetMotionSensorX() {
    int accelX = accelPadMode ? (inputIsHeld(BUTTON_TOUCH) ? 160 - inputGetTouch().x : 0) : 0;
    return 2047 + accelX;
}

int inputGetMotionSensorY() {
    int accelY = accelPadMode ? (inputIsHeld(BUTTON_TOUCH) ? 120 - inputGetTouch().y : 0) : 0;
    return 2047 + accelY;
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
