#include <stdio.h>

#include "console.h"
#include "gbmanager.h"
#include "input.h"
#include "gfx.h"
#include "menu.h"

#include <ctrcommon/platform.hpp>

int main(int argc, char* argv[]) {
    if(!platformInit()) {
        return 0;
    }

    gfxInit();
    consoleInitScreens();

    mgr_init();
    setMenuDefaults();
    readConfigFile();

    printf("GameYob 3DS\n\n");

    mgr_selectRom();
    while(true) {
        mgr_runFrame();
        mgr_updateVBlank();
    }

    return 0;
}
