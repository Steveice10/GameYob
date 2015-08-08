#include <sys/stat.h>
#include <string.h>

#include <algorithm>

#include "platform/system.h"
#include "cheatengine.h"
#include "gameboy.h"
#include "romfile.h"

#define TO_INT(a) ( (a) >= 'a' ? (a) - 'a' + 10 : (a) >= 'A' ? (a) - 'A' + 10 : (a) - '0')

CheatEngine::CheatEngine(Gameboy* g) {
    gameboy = g;
    cheatsEnabled = true;
    numCheats = 0;
}

void CheatEngine::enableCheats(bool enable) {
    cheatsEnabled = enable;
}

bool CheatEngine::addCheat(const char* str) {
    int len;
    int i = numCheats;

    if(i == MAX_CHEATS) {
        return false;
    }

    // Clear all flags
    cheats[i].flags = 0;
    cheats[i].patchedBanks = std::vector<int>();
    cheats[i].patchedValues = std::vector<int>();

    len = strlen(str);
    strncpy(cheats[i].cheatString, str, 12);

    // GameGenie AAA-BBB-CCC
    if(len == 11) {
        cheats[i].flags |= CHEAT_FLAG_GAMEGENIE;

        cheats[i].data = TO_INT(str[0]) << 4 | TO_INT(str[1]);
        cheats[i].address = TO_INT(str[6]) << 12 | TO_INT(str[2]) << 8 | TO_INT(str[4]) << 4 | TO_INT(str[5]);
        cheats[i].compare = TO_INT(str[8]) << 4 | TO_INT(str[10]);

        cheats[i].address ^= 0xf000;
        cheats[i].compare = (cheats[i].compare >> 2) | (cheats[i].compare & 0x3) << 6;
        cheats[i].compare ^= 0xba;

        systemPrintDebug("GG %04x / %02x -> %02x\n", cheats[i].address, cheats[i].data, cheats[i].compare);
    } else if(len == 7) { // GameGenie (6digit version) AAA-BBB
        cheats[i].flags |= CHEAT_FLAG_GAMEGENIE1;

        cheats[i].data = TO_INT(str[0]) << 4 | TO_INT(str[1]);
        cheats[i].address = TO_INT(str[6]) << 12 | TO_INT(str[2]) << 8 | TO_INT(str[4]) << 4 | TO_INT(str[5]);

        systemPrintDebug("GG1 %04x / %02x\n", cheats[i].address, cheats[i].data);
    } else if(len == 8) { // Gameshark AAAAAAAA
        cheats[i].flags |= CHEAT_FLAG_GAMESHARK;

        cheats[i].data = TO_INT(str[2]) << 4 | TO_INT(str[3]);
        cheats[i].bank = TO_INT(str[0]) << 4 | TO_INT(str[1]);
        cheats[i].address = TO_INT(str[6]) << 12 | TO_INT(str[7]) << 8 | TO_INT(str[4]) << 4 | TO_INT(str[5]);

        systemPrintDebug("GS (%02x)%04x/ %02x\n", cheats[i].bank, cheats[i].address, cheats[i].data);
    } else { // dafuq did i just read ?
        return false;
    }

    numCheats++;
    return true;
}

void CheatEngine::toggleCheat(int i, bool enabled) {
    if(enabled) {
        cheats[i].flags |= CHEAT_FLAG_ENABLED;
        if((cheats[i].flags & CHEAT_FLAG_TYPE_MASK) != CHEAT_FLAG_GAMESHARK) {
            for(int j = 0; j < gameboy->getRomFile()->getRomBanks(); j++) {
                applyGGCheatsToBank(j);
            }
        }
    } else {
        unapplyGGCheat(i);
        cheats[i].flags &= ~CHEAT_FLAG_ENABLED;
    }
}

void CheatEngine::unapplyGGCheat(int cheat) {
    if((cheats[cheat].flags & CHEAT_FLAG_TYPE_MASK) != CHEAT_FLAG_GAMESHARK) {
        for(unsigned int i = 0; i < cheats[cheat].patchedBanks.size(); i++) {
            u8* bank = gameboy->getRomFile()->getRomBank(cheats[cheat].patchedBanks[i]);
            bank[cheats[cheat].address & 0x3fff] = (u8) cheats[cheat].patchedValues[i];
        }

        cheats[cheat].patchedBanks = std::vector<int>();
        cheats[cheat].patchedValues = std::vector<int>();
    }
}

void CheatEngine::applyGGCheatsToBank(int bank) {
    u8* bankPtr = gameboy->getRomFile()->getRomBank(bank);
    for(int i = 0; i < numCheats; i++) {
        if(cheats[i].flags & CHEAT_FLAG_ENABLED && ((cheats[i].flags & CHEAT_FLAG_TYPE_MASK) != CHEAT_FLAG_GAMESHARK)) {
            int bankSlot = cheats[i].address / 0x4000;
            if((bankSlot == 0 && bank == 0) || (bankSlot == 1 && bank != 0)) {
                int address = cheats[i].address & 0x3fff;
                if(((cheats[i].flags & CHEAT_FLAG_TYPE_MASK) == CHEAT_FLAG_GAMEGENIE1 || bankPtr[address] == cheats[i].compare) && std::find(cheats[i].patchedBanks.begin(), cheats[i].patchedBanks.end(), bank) == cheats[i].patchedBanks.end()) {
                    cheats[i].patchedBanks.push_back(bank);
                    cheats[i].patchedValues.push_back(bankPtr[address]);
                    bankPtr[address] = cheats[i].data;
                }
            }
        }
    }
}

void CheatEngine::applyGSCheats() {
    for(int i = 0; i < numCheats; i++) {
        if(cheats[i].flags & CHEAT_FLAG_ENABLED && ((cheats[i].flags & CHEAT_FLAG_TYPE_MASK) == CHEAT_FLAG_GAMESHARK)) {
            int compareBank = gameboy->getWramBank();
            switch(cheats[i].bank & 0xf0) {
                case 0x90:
                    gameboy->setWramBank(cheats[i].bank & 0x7);
                    gameboy->writeMemory(cheats[i].address, cheats[i].data);
                    gameboy->setWramBank(compareBank);
                    break;
                case 0x80: /* TODO : Find info and stuff */
                    break;
                case 0x00:
                    gameboy->writeMemory(cheats[i].address, cheats[i].data);
                    break;
            }
        }
    }
}

void CheatEngine::loadCheats(const char* filename) {
    numCheats = 0;

    // Begin loading new cheat file
    FILE* file = fopen(filename, "r");
    if(file == NULL) {
        return;
    }

    struct stat s;
    fstat(fileno(file), &s);
    while(ftell(file) < s.st_size) {
        int i = numCheats;

        char line[100];
        fgets(line, 100, file);

        if(*line != '\0') {
            char* spacePos = strchr(line, ' ');
            if(spacePos != NULL) {
                *spacePos = '\0';
                if(strlen(spacePos + 1) >= 1 && addCheat(line)) {
                    strncpy(cheats[i].name, spacePos + 2, MAX_CHEAT_NAME_LEN);
                    cheats[i].name[MAX_CHEAT_NAME_LEN] = '\0';
                    char c;
                    while((c = cheats[i].name[strlen(cheats[i].name) - 1]) == '\n' || c == '\r') {
                        cheats[i].name[strlen(cheats[i].name) - 1] = '\0';
                    }

                    toggleCheat(i, *(spacePos + 1) == '1');
                }
            }
        }
    }

    fclose(file);
}

void CheatEngine::saveCheats(const char* filename) {
    if(numCheats == 0) {
        return;
    }

    FILE* file = fopen(filename, "w");
    for(int i = 0; i < numCheats; i++) {
        fprintf(file, "%s %d%s\n", cheats[i].cheatString, (cheats[i].flags & CHEAT_FLAG_ENABLED) != 0, cheats[i].name);
    }

    fclose(file);
}