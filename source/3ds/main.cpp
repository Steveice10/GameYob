#include "system.h"
#include "gbmanager.h"

int main(int argc, char* argv[]) {
    if(!systemInit()) {
        return false;
    }

    mgr_selectRom();
    while(true) {
        mgr_runFrame();
        mgr_updateVBlank();
    }

    return 0;
}
