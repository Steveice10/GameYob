#include "platform/common/config.h"
#include "platform/common/manager.h"
#include "platform/common/menu.h"
#include "platform/system.h"

int main(int argc, char* argv[]) {
    if(!systemInit(argc, argv)) {
        return false;
    }

    mgrInit();
    setMenuDefaults();
    readConfigFile();

    while(systemIsRunning()) {
        mgrRun();
    }

    mgrExit();

    systemExit();
    return 0;
}
