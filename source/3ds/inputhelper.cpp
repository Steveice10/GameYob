#include <stdlib.h>

#include "inputhelper.h"
#include "console.h"
#include "gbmanager.h"
#include "gfx.h"

#include <ctrcommon/input.hpp>
#include <ctrcommon/platform.hpp>

u32 keysForceReleased = 0;
u32 repeatStartTimer = 0;
int repeatTimer = 0;

u8 buttonsPressed;

bool fastForwardMode = false; // controlled by the toggle hotkey
bool fastForwardKey = false;  // only while its hotkey is pressed

void flushFatCache() {
}

bool keyPressed(int key) {
    return inputIsHeld((Button) key) && !(keysForceReleased & key);
}

bool keyJustPressed(int key) {
    return inputIsPressed((Button) key) && !(keysForceReleased & key);
}

bool keyPressedAutoRepeat(int key) {
    if (keyJustPressed(key)) {
        repeatStartTimer = 14;
        return true;
    }
    if (keyPressed(key) && repeatStartTimer == 0 && repeatTimer == 0) {
        repeatTimer = 2;
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

    if(repeatTimer > 0) {
        repeatTimer--;
    }

    if(repeatStartTimer > 0) {
        repeatStartTimer--;
    }
}

int system_getMotionSensorX() {
    return 0;
}

int system_getMotionSensorY() {
    return 0;
}

void system_checkPolls() {
    if(!platformIsRunning()) {
        system_cleanup();
        exit(0);
    }
}

void system_waitForVBlank() {
    gfxWaitForVBlank();
}

void system_cleanup() {
    mgr_save();
    mgr_exit();

    consoleCleanup();
    gfxCleanup();
    platformCleanup();
}
