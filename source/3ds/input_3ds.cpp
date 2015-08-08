#ifdef BACKEND_3DS

#include <stdlib.h>
#include <string.h>

#include "platform/gfx.h"
#include "platform/input.h"
#include "platform/ui.h"
#include "ui/config.h"
#include "ui/filechooser.h"
#include "ui/menu.h"
#include "gameboy.h"
#include "romfile.h"

#include <ctrcommon/gpu.hpp>
#include <ctrcommon/input.hpp>
#include <ctrcommon/platform.hpp>

const char* dsKeyNames[NUM_BUTTONS] = {
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

UIKey uiKeyMapping[NUM_BUTTONS] = {
        UI_KEY_A,     // 0 = BUTTON_A
        UI_KEY_B,     // 1 = BUTTON_B
        UI_KEY_NONE,  // 2 = BUTTON_SELECT
        UI_KEY_NONE,  // 3 = BUTTON_START
        UI_KEY_RIGHT, // 4 = BUTTON_DRIGHT
        UI_KEY_LEFT,  // 5 = BUTTON_DLEFT
        UI_KEY_UP,    // 6 = BUTTON_DUP
        UI_KEY_DOWN,  // 7 = BUTTON_DDOWN
        UI_KEY_R,     // 8 = BUTTON_R
        UI_KEY_L,     // 9 = BUTTON_L
        UI_KEY_X,     // 10 = BUTTON_X
        UI_KEY_Y,     // 11 = BUTTON_Y
        UI_KEY_NONE,  // 12 = BUTTON_NONE
        UI_KEY_NONE,  // 13 = BUTTON_NONE
        UI_KEY_NONE,  // 14 = BUTTON_ZL
        UI_KEY_NONE,  // 15 = BUTTON_ZR
        UI_KEY_NONE,  // 16 = BUTTON_NONE
        UI_KEY_NONE,  // 17 = BUTTON_NONE
        UI_KEY_NONE,  // 18 = BUTTON_NONE
        UI_KEY_NONE,  // 19 = BUTTON_NONE
        UI_KEY_NONE,  // 20 = BUTTON_TOUCH
        UI_KEY_NONE,  // 21 = BUTTON_NONE
        UI_KEY_NONE,  // 22 = BUTTON_NONE
        UI_KEY_NONE,  // 23 = BUTTON_NONE
        UI_KEY_RIGHT, // 24 = BUTTON_CSTICK_RIGHT
        UI_KEY_LEFT,  // 25 = BUTTON_CSTICK_LEFT
        UI_KEY_UP,    // 26 = BUTTON_CSTICK_UP
        UI_KEY_DOWN,  // 27 = BUTTON_CSTICK_DOWN
        UI_KEY_RIGHT, // 28 = BUTTON_CPAD_RIGHT
        UI_KEY_LEFT,  // 29 = BUTTON_CPAD_LEFT
        UI_KEY_UP,    // 30 = BUTTON_CPAD_UP
        UI_KEY_DOWN   // 31 = BUTTON_CPAD_DOWN
};

int funcKeyMapping[NUM_FUNC_KEYS];

bool forceReleased[NUM_FUNC_KEYS] = {false};

u64 nextRepeat = 0;
u64 nextUiRepeat = 0;

extern void uiClearInput();
extern void uiPushInput(UIKey key);

void inputInit() {
}

void inputCleanup() {
}

void inputUpdate() {
    inputPoll();
    for(int i = 0; i < NUM_FUNC_KEYS; i++) {
        if(!inputKeyHeld(i)) {
            forceReleased[i] = false;
        }
    }

    if(accelPadMode && inputIsPressed(BUTTON_TOUCH) && inputGetTouch().x <= gputGetStringWidth("Exit", 8) && inputGetTouch().y <= gputGetStringHeight("Exit", 8)) {
        inputKeyRelease(BUTTON_TOUCH);
        accelPadMode = false;
        uiClear();
    }

    if(isMenuOn() || isFileChooserActive() || (gameboy->isRomLoaded() && gameboy->getRomFile()->isGBS())) {
        for(int i = 0; i < NUM_BUTTONS; i++) {
            Button button = (Button) (1 << i);
            bool pressed = false;
            if(inputIsPressed(button)) {
                nextUiRepeat = platformGetTime() + 250;
                pressed = true;
            } else if(inputIsHeld(button) && platformGetTime() >= nextUiRepeat) {
                nextUiRepeat = platformGetTime() + 50;
                pressed = true;
            }

            if(pressed) {
                UIKey key = uiKeyMapping[i];
                if(key != UI_KEY_NONE) {
                    uiPushInput(key);
                }
            }
        }
    }
}

const char* inputGetKeyName(int keyIndex) {
    return dsKeyNames[keyIndex];
}

bool inputIsValidKey(int keyIndex) {
    return keyIndex >= 0 && keyIndex < NUM_BUTTONS && (keyIndex <= 11 || keyIndex == 14 || keyIndex == 15 || keyIndex >= 24);
}

bool inputKeyHeld(int key) {
    if(key < 0 || key >= NUM_FUNC_KEYS) {
        return false;
    }

    if(forceReleased[key]) {
        return false;
    }

    return inputIsHeld((Button) funcKeyMapping[key]);
}

bool inputKeyPressed(int key) {
    if(key < 0 || key >= NUM_FUNC_KEYS) {
        return false;
    }

    if(forceReleased[key]) {
        return false;
    }

    return inputIsPressed((Button) funcKeyMapping[key]);
}

bool inputKeyRepeat(int key) {
    if(key < 0 || key >= NUM_FUNC_KEYS) {
        return false;
    }

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
    if(key < 0 || key >= NUM_FUNC_KEYS) {
        return;
    }

    forceReleased[key] = true;
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
    memset(funcKeyMapping, 0, NUM_FUNC_KEYS * sizeof(int));
    for(int i = 0; i < NUM_BUTTONS; i++) {
        funcKeyMapping[keyConfig->funcKeys[i]] |= (1 << i);
    }

    funcKeyMapping[FUNC_KEY_MENU] |= BUTTON_TOUCH;
}

#endif