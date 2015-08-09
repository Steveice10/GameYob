#include "platform/common/config.h"
#include "platform/system.h"

int main(int argc, char* argv[]) {
    if(!systemInit(argc, argv)) {
        return false;
    }

    systemRun();
    return 0;
}
