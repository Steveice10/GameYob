#include <string.h>

#include "platform/system.h"
#include "ui/config.h"

int main(int argc, char* argv[]) {
    if(!systemInit(argc, argv)) {
        return false;
    }

    systemRun();
    return 0;
}
