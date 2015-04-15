#include <3ds.h>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <string>
#include <ctrcommon/platform.hpp>

#include "inputhelper.h"
#include "gameboy.h"
#include "main.h"
#include "console.h"
#include "menu.h"
#include "nifi.h"
#include "gbgfx.h"
#include "soundengine.h"
#include "cheats.h"
#include "gbs.h"
#include "filechooser.h"
#include "romfile.h"
#include "io.h"
#include "gbmanager.h"
#include "3dsgfx.h"

u32 lastKeysPressed = 0;
u32 keysPressed = 0;
u32 keysJustPressed = 0;

u32 keysForceReleased=0;
u32 repeatStartTimer=0;
int repeatTimer=0;

u8 buttonsPressed;

bool fastForwardMode = false; // controlled by the toggle hotkey
bool fastForwardKey = false;  // only while its hotkey is pressed

int rumbleInserted = 0;


void initInput()
{
}

void flushFatCache() {
}


bool keyPressed(int key) {
    return keysPressed & key;
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
bool keyJustPressed(int key) {
    return keysJustPressed & key;
}

void forceReleaseKey(int key) {
    keysForceReleased |= key;
    keysPressed &= ~key;
}

void inputUpdateVBlank() {
    hidScanInput();
    lastKeysPressed = keysPressed;
    keysPressed = hidKeysHeld();

    for (int i=0; i<32; i++) {
        if (keysForceReleased & (1<<i)) {
            if (!(keysPressed & (1<<i)))
                keysForceReleased &= ~(1<<i);
        }
    }
    keysPressed &= ~keysForceReleased;

    keysJustPressed = (lastKeysPressed ^ keysPressed) & keysPressed;

    if (repeatTimer > 0)
        repeatTimer--;
    if (repeatStartTimer > 0)
        repeatStartTimer--;
}

void system_doRumble(bool rumbleVal)
{
}

int system_getMotionSensorX() {
    return 0;
}
int system_getMotionSensorY() {
    return 0;
}


void system_checkPolls() {
    if(!aptMainLoop()) {
        system_cleanup();
        exit(0);
    }
}

void system_waitForVBlank() {
    gfxFlushBuffers();
    if (!(fastForwardMode || fastForwardKey))
        gspWaitForVBlank();

    gfxMySwapBuffers();
    consoleCheckFramebuffers();
}

void system_cleanup() {
    mgr_save();
    mgr_exit();

    csndExit();

    //fsExit();
    //gfxExit();
    //hidExit();
    //aptExit();
    //srvExit();
    platformCleanup();
}
