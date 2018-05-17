#ifdef BACKEND_SWITCH

#include <string.h>
#include <time.h>

#include <switch.h>

#include "platform/common/config.h"
#include "platform/common/menu/filechooser.h"
#include "platform/common/menu/menu.h"
#include "platform/input.h"
#include "platform/ui.h"

#define NUM_BUTTONS 27

static const char* switchKeyNames[NUM_BUTTONS] = {
        "A",                 // 0
        "B",                 // 1
        "X",                 // 2
        "Y",                 // 3
        "Left Stick",        // 4
        "Right Stick",       // 5
        "L",                 // 6
        "R",                 // 7
        "ZL",                // 8
        "ZR",                // 9
        "+",                 // 10
        "-",                 // 11
        "D-Pad Left",        // 12
        "D-Pad Up",          // 13
        "D-Pad Right",       // 14
        "D-Pad Down",        // 15
        "Left Stick Left",   // 16
        "Left Stick Up",     // 17
        "Left Stick Right",  // 18
        "Left Stick Down",   // 19
        "Right Stick Left",  // 20
        "Right Stick Up",    // 21
        "Right Stick Right", // 22
        "Right Stick Down",  // 23
        "SL",                // 24
        "SR",                // 25
        "Touch",             // 26
};

static KeyConfig defaultKeyConfig = {
        "Main",
        {
                FUNC_KEY_A,                   // 0 = KEY_A
                FUNC_KEY_B,                   // 1 = KEY_B
                FUNC_KEY_START,               // 2 = KEY_X
                FUNC_KEY_SELECT,              // 3 = KEY_Y
                FUNC_KEY_NONE,                // 4 = KEY_LSTICK
                FUNC_KEY_NONE,                // 5 = KEY_RSTICK
                FUNC_KEY_FAST_FORWARD,        // 6 = KEY_L
                FUNC_KEY_MENU,                // 7 = KEY_R
                FUNC_KEY_FAST_FORWARD_TOGGLE, // 8 = KEY_ZL
                FUNC_KEY_MENU,                // 9 = KEY_ZR
                FUNC_KEY_START,               // 10 = KEY_PLUS
                FUNC_KEY_SELECT,              // 11 = KEY_MINUS
                FUNC_KEY_LEFT,                // 12 = KEY_DLEFT
                FUNC_KEY_UP,                  // 13 = KEY_DUP
                FUNC_KEY_RIGHT,               // 14 = KEY_DRIGHT
                FUNC_KEY_DOWN,                // 15 = KEY_DDOWN
                FUNC_KEY_LEFT,                // 16 = KEY_LSTICK_LEFT
                FUNC_KEY_UP,                  // 17 = KEY_LSTICK_UP
                FUNC_KEY_RIGHT,               // 18 = KEY_LSTICK_RIGHT
                FUNC_KEY_DOWN,                // 19 = KEY_LSTICK_DOWN
                FUNC_KEY_LEFT,                // 20 = KEY_RSTICK_LEFT
                FUNC_KEY_UP,                  // 21 = KEY_RSTICK_UP
                FUNC_KEY_RIGHT,               // 22 = KEY_RSTICK_RIGHT
                FUNC_KEY_DOWN,                // 23 = KEY_RSTICK_DOWN
                FUNC_KEY_NONE,                // 24 = KEY_SL
                FUNC_KEY_NONE,                // 25 = KEY_SR
                FUNC_KEY_NONE,                // 26 = KEY_TOUCH
        }
};

static UIKey uiKeyMapping[NUM_BUTTONS] = {
        UI_KEY_A,     // 0 = KEY_A
        UI_KEY_B,     // 1 = KEY_B
        UI_KEY_X,     // 2 = KEY_X
        UI_KEY_Y,     // 3 = KEY_Y
        UI_KEY_NONE,  // 4 = KEY_LSTICK
        UI_KEY_NONE,  // 5 = KEY_RSTICK
        UI_KEY_L,     // 6 = KEY_L
        UI_KEY_R,     // 7 = KEY_R
        UI_KEY_L,     // 8 = KEY_ZL
        UI_KEY_R,     // 9 = KEY_ZR
        UI_KEY_NONE,  // 10 = KEY_PLUS
        UI_KEY_NONE,  // 11 = KEY_MINUS
        UI_KEY_LEFT,  // 12 = KEY_DLEFT
        UI_KEY_UP,    // 13 = KEY_DUP
        UI_KEY_RIGHT, // 14 = KEY_DRIGHT
        UI_KEY_DOWN,  // 15 = KEY_DDOWN
        UI_KEY_LEFT,  // 16 = KEY_LSTICK_LEFT
        UI_KEY_UP,    // 17 = KEY_LSTICK_UP
        UI_KEY_RIGHT, // 18 = KEY_LSTICK_RIGHT
        UI_KEY_DOWN,  // 19 = KEY_LSTICK_DOWN
        UI_KEY_LEFT,  // 20 = KEY_RSTICK_LEFT
        UI_KEY_UP,    // 21 = KEY_RSTICK_UP
        UI_KEY_RIGHT, // 22 = KEY_RSTICK_RIGHT
        UI_KEY_DOWN,  // 23 = KEY_RSTICK_DOWN
        UI_KEY_NONE,  // 24 = KEY_SL
        UI_KEY_NONE,  // 25 = KEY_SR
        UI_KEY_NONE,  // 26 = KEY_TOUCH
};

static u64 funcKeyMapping[NUM_FUNC_KEYS];

static bool forceReleased[NUM_FUNC_KEYS] = {false};
static bool uiForceReleased[NUM_BUTTONS] = {false};

static time_t nextUiRepeat = 0;

extern void uiPushInput(UIKey key);

void inputInit() {
}

void inputCleanup() {
}

void inputUpdate() {
    hidScanInput();

    u64 down = hidKeysDown(CONTROLLER_HANDHELD);
    u64 held = hidKeysHeld(CONTROLLER_HANDHELD);

    for(u32 i = 0; i < NUM_FUNC_KEYS; i++) {
        if(!(held & funcKeyMapping[i])) {
            forceReleased[i] = false;
        }
    }

    for(u32 i = 0; i < NUM_BUTTONS; i++) {
        if(!(held & (1U << i))) {
            uiForceReleased[i] = false;
        }
    }

    if(menuOn || isFileChooserActive()) {
        for(u32 i = 0; i < NUM_BUTTONS; i++) {
            if(uiForceReleased[i]) {
                continue;
            }

            time_t currTime = time(NULL);

            u64 button = 1U << i;
            bool pressed = false;
            if(down & button) {
                nextUiRepeat = currTime + 250;
                pressed = true;
            } else if((held & button) && currTime >= nextUiRepeat) {
                nextUiRepeat = currTime + 50;
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

bool inputKeyHeld(int key) {
    return key >= 0 && key < NUM_FUNC_KEYS && !forceReleased[key] && (hidKeysHeld(CONTROLLER_HANDHELD) & funcKeyMapping[key]) != 0;
}

bool inputKeyPressed(int key) {
    return key >= 0 && key < NUM_FUNC_KEYS && !forceReleased[key] && (hidKeysDown(CONTROLLER_HANDHELD) & funcKeyMapping[key]) != 0;
}

void inputKeyRelease(int key) {
    if(key >= 0 && key < NUM_FUNC_KEYS) {
        forceReleased[key] = true;
    }
}

void inputReleaseAll() {
    for(int i = 0; i < NUM_FUNC_KEYS; i++) {
        inputKeyRelease(i);
    }

    for(int i = 0; i < NUM_BUTTONS; i++) {
        uiForceReleased[i] = true;
    }
}

u16 inputGetMotionSensorX() {
    return 0x7FF; // TODO
}

u16 inputGetMotionSensorY() {
    return 0x7FF; // TODO
}

int inputGetKeyCount() {
    return NUM_BUTTONS;
}

bool inputIsValidKey(int keyIndex) {
    return keyIndex >= 0 && keyIndex < NUM_BUTTONS && strlen(switchKeyNames[keyIndex]) > 0;
}

const char* inputGetKeyName(int keyIndex) {
    return switchKeyNames[keyIndex];
}

KeyConfig inputGetDefaultKeyConfig() {
    return defaultKeyConfig;
}

void inputLoadKeyConfig(KeyConfig* keyConfig) {
    memset(funcKeyMapping, 0, NUM_FUNC_KEYS * sizeof(u64));
    for(u32 i = 0; i < NUM_BUTTONS; i++) {
        funcKeyMapping[keyConfig->funcKeys[i]] |= (1U << i);
    }

    funcKeyMapping[FUNC_KEY_MENU] |= KEY_TOUCH;
}

#endif