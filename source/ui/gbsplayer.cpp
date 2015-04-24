#include <stdio.h>

#include "platform/input.h"
#include "ui/config.h"
#include "ui/manager.h"

u8 selectedSong;
int playingSong;

void gbsPlayerReset() {
    selectedSong = 0;
    playingSong = 0;
}

void gbsPlayerRefresh() {
    if(inputKeyRepeat(inputMapMenuKey(MENU_KEY_LEFT))) {
        if(selectedSong == 0) {
            selectedSong = gameboy->getRomFile()->getGBS()->getSongCount() - (u8) 1;
        } else {
            selectedSong--;
        }
    }

    if(inputKeyRepeat(inputMapMenuKey(MENU_KEY_RIGHT))) {
        selectedSong++;
        if(selectedSong == gameboy->getRomFile()->getGBS()->getSongCount()) {
            selectedSong = 0;
        }
    }

    if(inputKeyPressed(inputMapMenuKey(MENU_KEY_A))) {
        gameboy->getRomFile()->getGBS()->playSong(gameboy, selectedSong);
        playingSong = selectedSong;
    }

    if(inputKeyPressed(inputMapMenuKey(MENU_KEY_B))) {
        gameboy->getRomFile()->getGBS()->stopSong(gameboy);
        playingSong = -1;
    }

    printf("\33[0;0H");

    printf("Song %d of %d\33[0K\n", selectedSong + 1, gameboy->getRomFile()->getGBS()->getSongCount());
    if(playingSong == -1) {
        printf("(Not playing)\33[0K\n\n");
    } else {
        printf("(Playing %d)\33[0K\n\n", playingSong + 1);
    }

    printf("%s\n", gameboy->getRomFile()->getGBS()->getTitle().c_str());
    printf("%s\n", gameboy->getRomFile()->getGBS()->getAuthor().c_str());
    printf("%s\n", gameboy->getRomFile()->getGBS()->getCopyright().c_str());
}