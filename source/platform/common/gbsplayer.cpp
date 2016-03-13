#include "platform/common/manager.h"
#include "platform/input.h"
#include "platform/ui.h"
#include "gameboy.h"
#include "romfile.h"

u8 selectedSong;
int playingSong;

void gbsPlayerReset() {
    selectedSong = 0;
    playingSong = gameboy->romFile->getGBS()->getFirstSong();

    gameboy->romFile->getGBS()->playSong(gameboy, playingSong);
}

void gbsPlayerDraw() {
    uiClear();

    uiPrint("Song %d of %d\n", selectedSong + 1, gameboy->romFile->getGBS()->getSongCount());
    if(playingSong == -1) {
        uiPrint("(Not playing)\n\n");
    } else {
        uiPrint("(Playing %d)\n\n", playingSong + 1);
    }

    uiPrint("%s\n", gameboy->romFile->getGBS()->getTitle().c_str());
    uiPrint("%s\n", gameboy->romFile->getGBS()->getAuthor().c_str());
    uiPrint("%s\n", gameboy->romFile->getGBS()->getCopyright().c_str());

    uiFlush();
}

void gbsPlayerRefresh() {
    bool redraw = false;

    UIKey key;
    while((key = uiReadKey()) != UI_KEY_NONE) {
        if(key == UI_KEY_LEFT) {
            if(selectedSong == 0) {
                selectedSong = gameboy->romFile->getGBS()->getSongCount() - (u8) 1;
            } else {
                selectedSong--;
            }

            redraw = true;
        }

        if(key == UI_KEY_RIGHT) {
            selectedSong++;
            if(selectedSong == gameboy->romFile->getGBS()->getSongCount()) {
                selectedSong = 0;
            }

            redraw = true;
        }

        if(key == UI_KEY_A) {
            gameboy->romFile->getGBS()->playSong(gameboy, selectedSong);
            playingSong = selectedSong;

            redraw = true;
        }

        if(key == UI_KEY_B) {
            gameboy->romFile->getGBS()->stopSong(gameboy);
            playingSong = -1;

            redraw = true;
        }
    }

    if(redraw) {
        gbsPlayerDraw();
    }
}