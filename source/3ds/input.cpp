#include <stdlib.h>
#include <string.h>

#include "platform/gfx.h"
#include "platform/input.h"
#include "ui/config.h"
#include "ui/menu.h"

#include <3ds.h>

#include <ctrcommon/input.hpp>
#include <ctrcommon/platform.hpp>
#include <ctrcommon/screen.hpp>

const char* dsKeyNames[] = {
        "A",         // 0
        "B",         // 1
        "Select",    // 2
        "Start",     // 3
        "Right",     // 4
        "Left",      // 5
        "Up",        // 6
        "Down",      // 7
        "R",         // 8
        "L",         // 9
        "X",         // 10
        "Y",         // 11
        "",          // 12
        "",          // 13
        "ZL",        // 14
        "ZR",        // 15
        "",          // 16
        "",          // 17
        "",          // 18
        "",          // 19
        "",          // 20
        "",          // 21
        "",          // 22
        "",          // 23
        "C-Right",   // 24
        "C-Left",    // 25
        "C-Up",      // 26
        "C-Down",    // 27
        "Pad-Right", // 28
        "Pad-Left",  // 29
        "Pad-Up",    // 30
        "Pad-Down"   // 31
};

static KeyConfig defaultKeyConfig = {
        "Main",
        {
                FUNC_KEY_A,            // 0 = BUTTON_A
                FUNC_KEY_B,            // 1 = BUTTON_B
                FUNC_KEY_SELECT,       // 2 = BUTTON_SELECT
                FUNC_KEY_START,        // 3 = BUTTON_START
                FUNC_KEY_RIGHT,        // 4 = BUTTON_DRIGHT
                FUNC_KEY_LEFT,         // 5 = BUTTON_DLEFT
                FUNC_KEY_UP,           // 6 = BUTTON_DUP
                FUNC_KEY_DOWN,         // 7 = BUTTON_DDOWN
                FUNC_KEY_MENU,         // 8 = BUTTON_R
                FUNC_KEY_FAST_FORWARD, // 9 = BUTTON_L
                FUNC_KEY_START,        // 10 = BUTTON_X
                FUNC_KEY_SELECT,       // 11 = BUTTON_Y
                FUNC_KEY_NONE,         // 12 = BUTTON_NONE
                FUNC_KEY_NONE,         // 13 = BUTTON_NONE
                FUNC_KEY_SCREENSHOT,   // 14 = BUTTON_ZL
                FUNC_KEY_SCALE,        // 15 = BUTTON_ZR
                FUNC_KEY_NONE,         // 16 = BUTTON_NONE
                FUNC_KEY_NONE,         // 17 = BUTTON_NONE
                FUNC_KEY_NONE,         // 18 = BUTTON_NONE
                FUNC_KEY_NONE,         // 19 = BUTTON_NONE
                FUNC_KEY_NONE,         // 20 = BUTTON_TOUCH
                FUNC_KEY_NONE,         // 21 = BUTTON_NONE
                FUNC_KEY_NONE,         // 22 = BUTTON_NONE
                FUNC_KEY_NONE,         // 23 = BUTTON_NONE
                FUNC_KEY_RIGHT,        // 24 = BUTTON_CSTICK_RIGHT
                FUNC_KEY_LEFT,         // 25 = BUTTON_CSTICK_LEFT
                FUNC_KEY_UP,           // 26 = BUTTON_CSTICK_UP
                FUNC_KEY_DOWN,         // 27 = BUTTON_CSTICK_DOWN
                FUNC_KEY_RIGHT,        // 28 = BUTTON_CPAD_RIGHT
                FUNC_KEY_LEFT,         // 29 = BUTTON_CPAD_LEFT
                FUNC_KEY_UP,           // 30 = BUTTON_CPAD_UP
                FUNC_KEY_DOWN          // 31 = BUTTON_CPAD_DOWN
        }
};

int keyMapping[NUM_FUNC_KEYS];

u32 keysForceReleased = 0;
u64 nextRepeat = 0;

void inputUpdate() {
    inputPoll();
    for(int i = 0; i < 32; i++) {
        if(!inputIsHeld((Button) (1 << i))) {
            keysForceReleased &= ~(1 << i);
        }
    }

    if(accelPadMode && inputIsPressed(BUTTON_TOUCH) && inputGetTouch().x <= screenGetStrWidth("Exit") && inputGetTouch().y <= screenGetStrHeight("Exit")) {
        inputKeyRelease(BUTTON_TOUCH);
        accelPadMode = false;
        consoleClear();
    }
}

const char* inputGetKeyName(int keyIndex) {
    return dsKeyNames[keyIndex];
}

bool inputIsValidKey(int keyIndex) {
    return keyIndex <= 11 || keyIndex == 14 || keyIndex == 15 || keyIndex >= 24;
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

KeyConfig inputGetDefaultKeyConfig() {
    return defaultKeyConfig;
}

void inputLoadKeyConfig(KeyConfig* keyConfig) {
    memset(keyMapping, 0, NUM_FUNC_KEYS * sizeof(int));
    for(int i = 0; i < NUM_BINDABLE_BUTTONS; i++) {
        keyMapping[keyConfig->funcKeys[i]] |= (1 << i);
    }

    keyMapping[FUNC_KEY_MENU] |= BUTTON_TOUCH;
}

int inputMapFuncKey(int funcKey) {
    return keyMapping[funcKey];
}

int inputMapMenuKey(int menuKey) {
    switch(menuKey) {
        case MENU_KEY_A:
            return BUTTON_A;
        case MENU_KEY_B:
            return BUTTON_B;
        case MENU_KEY_LEFT:
            return BUTTON_LEFT;
        case MENU_KEY_RIGHT:
            return BUTTON_RIGHT;
        case MENU_KEY_UP:
            return BUTTON_UP;
        case MENU_KEY_DOWN:
            return BUTTON_DOWN;
        case MENU_KEY_R:
            return BUTTON_R;
        case MENU_KEY_L:
            return BUTTON_L;
        case MENU_KEY_X:
            return BUTTON_X;
        case MENU_KEY_Y:
            return BUTTON_Y;
    }

    return 0;
}