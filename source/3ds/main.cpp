#include <stdio.h>
#include "3dsgfx.h"
#include "inputhelper.h"
#include "console.h"
#include "menu.h"
#include "gbmanager.h"

#include <ctrcommon/platform.hpp>

int main(int argc, char* argv[])
{
    if(!platformInit()) {
        return 0;
    }

    initGPU();

    consoleInitBottom();

    fs_init();
    mgr_init();

	initInput();
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
