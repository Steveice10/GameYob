#ifdef BACKEND_SDL

#include <string.h>

#include <vector>

#include "platform/common/config.h"
#include "platform/input.h"

#include <SDL2/SDL_events.h>

static KeyConfig defaultKeyConfig = {
        "Main",
        {FUNC_KEY_NONE}
};

static std::vector<int> bindings[NUM_FUNC_KEYS];

static bool pressed[NUM_FUNC_KEYS] = {false};
static bool held[NUM_FUNC_KEYS] = {false};
static bool forceReleased[NUM_FUNC_KEYS] = {false};

static const Uint8* keyState = NULL;
static int keyCount = 0;

void inputInit() {
    keyState = SDL_GetKeyboardState(&keyCount);

    defaultKeyConfig.funcKeys[SDL_SCANCODE_Z] = FUNC_KEY_A;
    defaultKeyConfig.funcKeys[SDL_SCANCODE_X] = FUNC_KEY_B;
    defaultKeyConfig.funcKeys[SDL_SCANCODE_LEFT] = FUNC_KEY_LEFT;
    defaultKeyConfig.funcKeys[SDL_SCANCODE_RIGHT] = FUNC_KEY_RIGHT;
    defaultKeyConfig.funcKeys[SDL_SCANCODE_UP] = FUNC_KEY_UP;
    defaultKeyConfig.funcKeys[SDL_SCANCODE_DOWN] = FUNC_KEY_DOWN;
    defaultKeyConfig.funcKeys[SDL_SCANCODE_RETURN] = FUNC_KEY_START;
    defaultKeyConfig.funcKeys[SDL_SCANCODE_RSHIFT] = FUNC_KEY_SELECT;
    defaultKeyConfig.funcKeys[SDL_SCANCODE_M] = FUNC_KEY_MENU;
    defaultKeyConfig.funcKeys[SDL_SCANCODE_P] = FUNC_KEY_MENU_PAUSE;
    defaultKeyConfig.funcKeys[SDL_SCANCODE_S] = FUNC_KEY_SAVE;
    defaultKeyConfig.funcKeys[SDL_SCANCODE_Q] = FUNC_KEY_AUTO_A;
    defaultKeyConfig.funcKeys[SDL_SCANCODE_W] = FUNC_KEY_AUTO_B;
    defaultKeyConfig.funcKeys[SDL_SCANCODE_LCTRL] = FUNC_KEY_FAST_FORWARD;
    defaultKeyConfig.funcKeys[SDL_SCANCODE_LALT] = FUNC_KEY_FAST_FORWARD_TOGGLE;
    defaultKeyConfig.funcKeys[SDL_SCANCODE_RCTRL] = FUNC_KEY_SCALE;
    defaultKeyConfig.funcKeys[SDL_SCANCODE_RALT] = FUNC_KEY_RESET;
    defaultKeyConfig.funcKeys[SDL_SCANCODE_BACKSPACE] = FUNC_KEY_SCREENSHOT;
}

void inputCleanup() {
}

void inputUpdate() {
    for(int funcKey = 0; funcKey < NUM_FUNC_KEYS; funcKey++) {
        bool currPressed = false;
        for(int realKey : bindings[funcKey]) {
            if(keyState[realKey] == 1) {
                currPressed = true;
                break;
            }
        }

        if(currPressed) {
            pressed[funcKey] = !held[funcKey];
            held[funcKey] = true;
        } else {
            pressed[funcKey] = false;
            held[funcKey] = currPressed;
            forceReleased[funcKey] = false;
        }
    }
}

bool inputKeyHeld(int key) {
    return key >= 0 && key < NUM_FUNC_KEYS && !forceReleased[key] && held[key];
}

bool inputKeyPressed(int key) {
    return key >= 0 && key < NUM_FUNC_KEYS && !forceReleased[key] && pressed[key];
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
}

u16 inputGetMotionSensorX() {
    return 0x7FF;
}

u16 inputGetMotionSensorY() {
    return 0x7FF;
}

int inputGetKeyCount() {
    return keyCount;
}

bool inputIsValidKey(int keyIndex) {
    return keyIndex >= 0 && keyIndex < keyCount && keyIndex != SDL_SCANCODE_RETURN2 && strlen(SDL_GetScancodeName((SDL_Scancode) keyIndex)) > 0;
}

const char* inputGetKeyName(int keyIndex) {
    return SDL_GetScancodeName((SDL_Scancode) keyIndex);
}

KeyConfig inputGetDefaultKeyConfig() {
    return defaultKeyConfig;
}

void inputLoadKeyConfig(KeyConfig* keyConfig) {
    for(int i = 0; i < NUM_FUNC_KEYS; i++) {
        bindings[i].clear();
    }

    for(int i = 0; i < keyCount; i++) {
        bindings[keyConfig->funcKeys[i]].push_back(i);
    }
}

#endif
