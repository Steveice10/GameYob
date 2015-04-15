#include <3ds.h>
#include <stdio.h>
#include <cwchar>
#include <3dsgfx.h>
#include <ctrcommon/platform.hpp>
#include "gbgfx.h"
#include "soundengine.h"
#include "inputhelper.h"
#include "gameboy.h"
#include "console.h"
#include "gbs.h"
#include "romfile.h"
#include "menu.h"
#include "gbmanager.h"
#include "printconsole.h"

void csnd_init();

int main(int argc, char* argv[])
{
    //srvInit();

    //aptInit();
    //hidInit(NULL);
    //gfxInitDefault();
    //fsInit();

    if(!platformInit()) {
        return 0;
    }

    initGPU();

    consoleInitBottom();

    fs_init();
    mgr_init();
    csnd_init();

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
