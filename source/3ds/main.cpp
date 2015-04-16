#include <stdio.h>

#include "gfx.h"
#include "console.h"
#include "inputhelper.h"
#include "menu.h"
#include "gbmanager.h"

#include <ctrcommon/platform.hpp>

int main(int argc, char* argv[])
{
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

    while(1) {
        mgr_runFrame();
        mgr_updateVBlank();
    }

	return 0;
}
