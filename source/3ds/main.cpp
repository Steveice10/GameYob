#include "system.h"

int main(int argc, char* argv[]) {
    if(!systemInit()) {
        return false;
    }

    systemRun();
    return 0;
}
