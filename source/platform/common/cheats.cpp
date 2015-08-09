#include <string.h>

#include "platform/common/cheats.h"
#include "platform/common/menu.h"
#include "platform/ui.h"
#include "cheatengine.h"
#include "gameboy.h"

int cheatsPerPage = 0;
int cheatMenuSelection = 0;
bool cheatMenuGameboyWasPaused = false;

void redrawCheatMenu() {
    cheatsPerPage = uiGetHeight() - 2;
    int numCheats = gameboy->getCheatEngine()->getNumCheats();
    int numPages = (numCheats - 1) / cheatsPerPage + 1;
    int page = cheatMenuSelection / cheatsPerPage;

    uiClear();

    uiPrint("          Cheat Menu      ");
    uiPrint("%d/%d\n\n", page + 1, numPages);
    for(int i = page * cheatsPerPage; i < numCheats && i < (page + 1) * cheatsPerPage; i++) {
        if(cheatMenuSelection == i) {
            uiSetLineHighlighted(true);
        }

        uiPrint("%s", gameboy->getCheatEngine()->cheats[i].name);

        for(unsigned int j = 0; j < 25 - strlen(gameboy->getCheatEngine()->cheats[i].name); j++) {
            uiPrint(" ");
        }

        if(gameboy->getCheatEngine()->isCheatEnabled(i)) {
            if(cheatMenuSelection == i) {
                uiSetTextColor(TEXT_COLOR_YELLOW);
                uiPrint("* ");
                uiSetTextColor(TEXT_COLOR_GREEN);
                uiPrint("On");
                uiSetTextColor(TEXT_COLOR_YELLOW);
                uiPrint("  *");
                uiSetTextColor(TEXT_COLOR_NONE);
            } else {
                uiSetTextColor(TEXT_COLOR_GREEN);
                uiPrint("  On   ");
                uiSetTextColor(TEXT_COLOR_NONE);
            }
        } else {
            if(cheatMenuSelection == i) {
                uiSetTextColor(TEXT_COLOR_YELLOW);
                uiPrint("* ");
                uiSetTextColor(TEXT_COLOR_RED);
                uiPrint("Off");
                uiSetTextColor(TEXT_COLOR_YELLOW);
                uiPrint(" *");
                uiSetTextColor(TEXT_COLOR_NONE);
            } else {
                uiSetTextColor(TEXT_COLOR_RED);
                uiPrint("  Off  ");
                uiSetTextColor(TEXT_COLOR_NONE);
            }
        }

        uiPrint("\n");

        uiSetLineHighlighted(false);
    }

    uiFlush();
}

void updateCheatMenu() {
    bool redraw = false;
    int numCheats = gameboy->getCheatEngine()->getNumCheats();

    if(cheatMenuSelection >= numCheats) {
        cheatMenuSelection = 0;
    }

    UIKey key;
    while((key = uiReadKey()) != UI_KEY_NONE) {
        if(key == UI_KEY_UP) {
            if(cheatMenuSelection > 0) {
                cheatMenuSelection--;
                redraw = true;
            }
        } else if(key == UI_KEY_DOWN) {
            if(cheatMenuSelection < numCheats - 1) {
                cheatMenuSelection++;
                redraw = true;
            }
        } else if(key == UI_KEY_RIGHT || key == UI_KEY_LEFT) {
            gameboy->getCheatEngine()->toggleCheat(cheatMenuSelection, !gameboy->getCheatEngine()->isCheatEnabled(cheatMenuSelection));
            redraw = true;
        } else if(key == UI_KEY_R) {
            cheatMenuSelection += cheatsPerPage;
            if(cheatMenuSelection >= numCheats) {
                cheatMenuSelection = 0;
            }

            redraw = true;
        } else if(key == UI_KEY_L) {
            cheatMenuSelection -= cheatsPerPage;
            if(cheatMenuSelection < 0) {
                cheatMenuSelection = numCheats - 1;
            }

            redraw = true;
        } else if(key == UI_KEY_B) {
            closeSubMenu();
            if(!cheatMenuGameboyWasPaused) {
                gameboy->unpause();
            }

            return;
        }
    }

    if(redraw) {
        redrawCheatMenu();
    }
}

void startCheatMenu() {
    cheatMenuGameboyWasPaused = gameboy->isGameboyPaused();
    gameboy->pause();

    displaySubMenu(updateCheatMenu);
    redrawCheatMenu();
}
