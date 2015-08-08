#include <string.h>

#include "platform/system.h"
#include "ui/config.h"

int main(int argc, char* argv[]) {
    strcpy(gbBiosPath, defaultGbBiosPath);
    strcpy(gbcBiosPath, defaultGbcBiosPath);
    strcpy(borderPath, defaultBorderPath);
    strcpy(romPath, defaultRomPath);

    if(!systemInit(argc, argv)) {
        return false;
    }

    systemRun();
    return 0;
}
