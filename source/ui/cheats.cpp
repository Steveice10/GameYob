#include <string.h>
#include <algorithm>

#include "platform/input.h"
#include "ui/config.h"
#include "ui/menu.h"
#include "cheatengine.h"
#include "gameboy.h"

const int cheatsPerPage = 18;
int cheatMenuSelection = 0;
bool cheatMenuGameboyWasPaused;

CheatEngine* cheatEngine; // cheat engine to display the menu for

void redrawCheatMenu() {
    int numCheats = cheatEngine->getNumCheats();
    int numPages = (numCheats - 1) / cheatsPerPage + 1;
    int page = cheatMenuSelection / cheatsPerPage;

    iprintf("\x1b[2J");

    printf("          Cheat Menu      ");
    printf("%d/%d\n\n", page + 1, numPages);
    for(int i = page * cheatsPerPage; i < numCheats && i < (page + 1) * cheatsPerPage; i++) {
        std::string nameColor = (cheatMenuSelection == i ? "\x1b[1m\x1b[33m" : "");
        printf((nameColor + cheatEngine->cheats[i].name + "\x1b[0m").c_str());
        for(unsigned int j = 0; j < 25 - strlen(cheatEngine->cheats[i].name); j++) {
            printf(" ");
        }

        if(cheatEngine->isCheatEnabled(i)) {
            if(cheatMenuSelection == i) {
                printf("\x1b[1m\x1b[33m* \x1b[0m");
                printf("\x1b[1m\x1b[32mOn\x1b[0m");
                printf("\x1b[1m\x1b[33m * \x1b[0m");
            } else {
                printf("  On   ");
            }
        } else {
            if(cheatMenuSelection == i) {
                printf("\x1b[1m\x1b[33m* \x1b[0m");
                printf("\x1b[1m\x1b[32mOff\x1b[0m");
                printf("\x1b[1m\x1b[33m *\x1b[0m");
            } else {
                printf("  Off  ");
            }
        }
    }

}

void updateCheatMenu() {
    bool redraw = false;
    int numCheats = cheatEngine->getNumCheats();

    if(cheatMenuSelection >= numCheats) {
        cheatMenuSelection = 0;
    }

    if(inputKeyRepeat(inputMapMenuKey(MENU_KEY_UP))) {
        if(cheatMenuSelection > 0) {
            cheatMenuSelection--;
            redraw = true;
        }
    } else if(inputKeyRepeat(inputMapMenuKey(MENU_KEY_DOWN))) {
        if(cheatMenuSelection < numCheats - 1) {
            cheatMenuSelection++;
            redraw = true;
        }
    } else if(inputKeyPressed(inputMapMenuKey(MENU_KEY_RIGHT)) || inputKeyPressed(inputMapMenuKey(MENU_KEY_LEFT))) {
        cheatEngine->toggleCheat(cheatMenuSelection, !cheatEngine->isCheatEnabled(cheatMenuSelection));
        redraw = true;
    } else if(inputKeyPressed(inputMapMenuKey(MENU_KEY_R))) {
        cheatMenuSelection += cheatsPerPage;
        if(cheatMenuSelection >= numCheats) {
            cheatMenuSelection = 0;
        }

        redraw = true;
    } else if(inputKeyPressed(inputMapMenuKey(MENU_KEY_L))) {
        cheatMenuSelection -= cheatsPerPage;
        if(cheatMenuSelection < 0) {
            cheatMenuSelection = numCheats - 1;
        }

        redraw = true;
    }

    if(inputKeyPressed(inputMapMenuKey(MENU_KEY_B))) {
        closeSubMenu();
        if(!cheatMenuGameboyWasPaused) {
            gameboy->unpause();
        }
    }

    if(redraw) {
        redrawCheatMenu();
    }
}

bool startCheatMenu() {
    cheatEngine = gameboy->getCheatEngine();
    if(cheatEngine == NULL || cheatEngine->getNumCheats() == 0) {
        return false;
    }

    cheatMenuGameboyWasPaused = gameboy->isGameboyPaused();
    gameboy->pause();
    displaySubMenu(updateCheatMenu);
    redrawCheatMenu();

    return true;
}
