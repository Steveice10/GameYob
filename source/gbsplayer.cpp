// GBS files contain music ripped from a game.
#include <stdio.h>

#include "platform/input.h"
#include "platform/system.h"
#include "ui/config.h"
#include "gameboy.h"

GBSPlayer::GBSPlayer(Gameboy* gb) {
    this->gameboy = gb;
}

// private

void GBSPlayer::gbsRedraw() {
    printf("\33[0;0H"); // Cursor to upper-left corner

    printf("Song %d of %d\33[0K\n", gbsSelectedSong + 1, gbsNumSongs);
    if(gbsPlayingSong == -1)
        printf("(Not playing)\33[0K\n\n");
    else
        printf("(Playing %d)\33[0K\n\n", gbsPlayingSong + 1);

    // Print music information
    for(int i = 0; i < 3; i++) {
        for(int j = 0; j < 32; j++) {
            char c = gbsHeader[0x10 + i * 0x20 + j];
            if(c == 0)
                printf(" ");
            else
                printf("%c", c);
        }
        printf("\n");
    }
}

void GBSPlayer::gbsLoadSong() {
    u8* romBank0 = gameboy->getRomFile()->getRomBank(0);
    gameboy->initMMU();
    gameboy->ime = 0;

    gameboy->gbRegs.sp.w = (*(gbsHeader + 0x0c) | *(gbsHeader + 0x0c+1)<<8);
    u8 tma = gbsHeader[0x0e];
    u8 tac = gbsHeader[0x0f];

    if(tac & 0x80)
        gameboy->setDoubleSpeed(1);
    tac &= ~0x80;
    if(tma == 0 && tac == 0) {
        // Vblank interrupt handler
        romBank0[0x40] = 0xcd; // call
        romBank0[0x41] = gbsPlayAddress & 0xff;
        romBank0[0x42] = gbsPlayAddress >> 8;
        romBank0[0x43] = 0xd9; // reti
        gameboy->writeIO(0xff, INT_VBLANK);
    }
    else {
        // Timer interrupt handler
        romBank0[0x50] = 0xcd; // call
        romBank0[0x51] = gbsPlayAddress & 0xff;
        romBank0[0x52] = gbsPlayAddress >> 8;
        romBank0[0x53] = 0xd9; // reti
        gameboy->writeIO(0xff, INT_TIMER);
    }

    gameboy->writeIO(0x05, 0x00);
    gameboy->writeIO(0x06, tma);
    gameboy->writeIO(0x07, tac);

    gbsPlayingSong = gbsSelectedSong;
    gameboy->gbRegs.af.b.h = gbsPlayingSong;
    gameboy->writeMemory(--gameboy->gbRegs.sp.w, 0x01);
    gameboy->writeMemory(--gameboy->gbRegs.sp.w, 0x00); // Will return to beginning
    gameboy->gbRegs.pc.w = gbsInitAddress;
}

// public

void GBSPlayer::gbsReadHeader() {
    gbsNumSongs = gbsHeader[0x04];
    gbsLoadAddress = (*(gbsHeader + 0x06) | *(gbsHeader + 0x06+1)<<8);
    gbsInitAddress = (*(gbsHeader + 0x08) | *(gbsHeader + 0x08+1)<<8);
    gbsPlayAddress = (*(gbsHeader + 0x0a) | *(gbsHeader + 0x0a+1)<<8);
}

void GBSPlayer::gbsInit() {
    u8* romSlot0 = gameboy->getRomFile()->getRomBank(0);

    u8 firstSong = gbsHeader[0x05] - 1;

    // RST vectors
    for(int i = 0; i < 8; i++) {
        u16 dest = gbsLoadAddress + i * 8;
        romSlot0[i * 8] = 0xc3; // jp
        romSlot0[i * 8 + 1] = dest & 0xff;
        romSlot0[i * 8 + 2] = dest >> 8;
    }

    // Interrupt handlers
    for(int i = 0; i < 5; i++) {
        romSlot0[0x40 + i * 8] = 0xd9; // reti
    }

    // Infinite loop
    romSlot0[0x100] = 0xfb; // ime
    romSlot0[0x101] = 0x76; // halt
    romSlot0[0x102] = 0x18; // jr -3
    romSlot0[0x103] = -3;

    gbsSelectedSong = firstSong;
    gbsLoadSong();
}

// Called at vblank each frame
void GBSPlayer::gbsCheckInput() {
    if(inputKeyRepeat(mapMenuKey(MENU_KEY_LEFT))) {
        if(gbsSelectedSong == 0)
            gbsSelectedSong = gbsNumSongs - 1;
        else
            gbsSelectedSong--;
    }
    if(inputKeyRepeat(mapMenuKey(MENU_KEY_RIGHT))) {
        gbsSelectedSong++;
        if(gbsSelectedSong == gbsNumSongs)
            gbsSelectedSong = 0;
    }
    if(inputKeyPressed(mapMenuKey(MENU_KEY_A))) {
        gbsLoadSong();
    }
    if(inputKeyPressed(mapMenuKey(MENU_KEY_B))) { // Stop playing music
        gbsPlayingSong = -1;
        gameboy->ime = 0;
        gameboy->writeIO(0xff, 0);
        gameboy->initSND();
    }
    gbsRedraw();

    if(gbsPlayingSong != -1)
        systemDisableSleepMode();
    else
        systemEnableSleepMode();
}
