#include "platform/common/manager.h"
#include "platform/input.h"
#include "platform/ui.h"
#include "gameboy.h"
#include "romfile.h"

u8 selectedSong;
int playingSong;

void gbsPlayerReset() {
    selectedSong = 0;
    playingSong = gameboy->getRomFile()->getGBS()->getFirstSong();
}

void gbsPlayerDraw() {
    uiClear();

    uiPrint("Song %d of %d\n", selectedSong + 1, gameboy->getRomFile()->getGBS()->getSongCount());
    if(playingSong == -1) {
        uiPrint("(Not playing)\n\n");
    } else {
        uiPrint("(Playing %d)\n\n", playingSong + 1);
    }

    uiPrint("%s\n", gameboy->getRomFile()->getGBS()->getTitle().c_str());
    uiPrint("%s\n", gameboy->getRomFile()->getGBS()->getAuthor().c_str());
    uiPrint("%s\n", gameboy->getRomFile()->getGBS()->getCopyright().c_str());

    uiFlush();
}

void gbsPlayerRefresh() {
    bool redraw = false;

    UIKey key;
    while((key = uiReadKey()) != UI_KEY_NONE) {
        if(key == UI_KEY_LEFT) {
            if(selectedSong == 0) {
                selectedSong = gameboy->getRomFile()->getGBS()->getSongCount() - (u8) 1;
            } else {
                selectedSong--;
            }

            redraw = true;
        }

        if(key == UI_KEY_RIGHT) {
            selectedSong++;
            if(selectedSong == gameboy->getRomFile()->getGBS()->getSongCount()) {
                selectedSong = 0;
            }

            redraw = true;
        }

        if(key == UI_KEY_A) {
            gameboy->getRomFile()->getGBS()->playSong(gameboy, selectedSong);
            playingSong = selectedSong;

            redraw = true;
        }

        if(key == UI_KEY_B) {
            gameboy->getRomFile()->getGBS()->stopSong(gameboy);
            playingSong = -1;

            redraw = true;
        }
    }

    if(redraw) {
        gbsPlayerDraw();
    }
}