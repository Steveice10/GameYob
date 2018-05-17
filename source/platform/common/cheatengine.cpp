#include <string.h>

#include <algorithm>
#include <fstream>
#include <sstream>

#include "libs/inih/INIReader.h"

#include "platform/common/cheatengine.h"
#include "platform/system.h"
#include "cartridge.h"
#include "gameboy.h"
#include "mmu.h"

#define TO_INT(a) ( (a) >= 'a' ? (a) - 'a' + 10 : (a) >= 'A' ? (a) - 'A' + 10 : (a) - '0')

CheatEngine::CheatEngine(Gameboy* g) {
    gameboy = g;
    numCheats = 0;
}

void CheatEngine::loadCheats(const std::string& filename) {
    numCheats = 0;

    ini_parse(filename.c_str(), CheatEngine::parseCheats, this);
}

void CheatEngine::saveCheats(const std::string& filename) {
    if(numCheats == 0) {
        return;
    }

    std::ofstream stream(filename);
    if(stream.is_open()) {
        for(int i = 0; i < numCheats; i++) {
            stream << "[" << cheats[i].name << "]\n";
            stream << "value=" << cheats[i].cheatString << "\n";
            stream << "enabled=" << ((cheats[i].flags & CHEAT_FLAG_ENABLED) != 0) << "\n";
            stream << "\n";
        }

        stream.close();
    }
}

int CheatEngine::parseCheats(void* user, const char* section, const char* name, const char* value) {
    CheatEngine* engine = (CheatEngine*) user;

    if(strcmp(name, "value") == 0) {
        engine->addCheat(std::string(section), std::string(value));
    } else if(strcmp(name, "enabled") == 0) {
        for(int i = 0; i < engine->numCheats; i++) {
            if(engine->cheats[i].name == section) {
                engine->toggleCheat(i, strcmp(value, "1") == 0);
                break;
            }
        }
    }

    return 1;
}

bool CheatEngine::addCheat(const std::string& name, const std::string& value) {
    int cheat = numCheats;

    if(cheat == MAX_CHEATS) {
        return false;
    }

    cheats[cheat].name = name;
    cheats[cheat].cheatString = value;

    // Clear all flags
    cheats[cheat].flags = 0;
    cheats[cheat].patchedBanks = std::vector<int>();
    cheats[cheat].patchedValues = std::vector<int>();

    size_t len = value.length();

    if(len == 11) { // GameGenie AAA-BBB-CCC
        cheats[cheat].flags |= CHEAT_FLAG_GAMEGENIE;

        cheats[cheat].data = (u8) (TO_INT(value[0]) << 4 | TO_INT(value[1]));
        cheats[cheat].address = (u16) (TO_INT(value[6]) << 12 | TO_INT(value[2]) << 8 | TO_INT(value[4]) << 4 | TO_INT(value[5]));
        cheats[cheat].compare = (u8) (TO_INT(value[8]) << 4 | TO_INT(value[10]));

        cheats[cheat].address ^= 0xf000;
        cheats[cheat].compare = (u8) ((cheats[cheat].compare >> 2) | (cheats[cheat].compare & 0x3) << 6);
        cheats[cheat].compare ^= 0xba;

        systemPrintDebug("GG %04x / %02x -> %02x\n", cheats[cheat].address, cheats[cheat].data, cheats[cheat].compare);
    } else if(len == 7) { // GameGenie (6digit version) AAA-BBB
        cheats[cheat].flags |= CHEAT_FLAG_GAMEGENIE1;

        cheats[cheat].data = (u8) (TO_INT(value[0]) << 4 | TO_INT(value[1]));
        cheats[cheat].address = (u16) (TO_INT(value[6]) << 12 | TO_INT(value[2]) << 8 | TO_INT(value[4]) << 4 | TO_INT(value[5]));

        systemPrintDebug("GG1 %04x / %02x\n", cheats[cheat].address, cheats[cheat].data);
    } else if(len == 8) { // Gameshark AAAAAAAA
        cheats[cheat].flags |= CHEAT_FLAG_GAMESHARK;

        cheats[cheat].data = (u8) (TO_INT(value[2]) << 4 | TO_INT(value[3]));
        cheats[cheat].bank = (u8) (TO_INT(value[0]) << 4 | TO_INT(value[1]));
        cheats[cheat].address = (u16) (TO_INT(value[6]) << 12 | TO_INT(value[7]) << 8 | TO_INT(value[4]) << 4 | TO_INT(value[5]));

        systemPrintDebug("GS (%02x)%04x/ %02x\n", cheats[cheat].bank, cheats[cheat].address, cheats[cheat].data);
    } else { // Unknown
        return false;
    }

    numCheats++;
    return true;
}

void CheatEngine::toggleCheat(int cheat, bool enabled) {
    if(gameboy->cartridge != NULL) {
        if(enabled) {
            cheats[cheat].flags |= CHEAT_FLAG_ENABLED;
            if((cheats[cheat].flags & CHEAT_FLAG_TYPE_MASK) != CHEAT_FLAG_GAMESHARK) {
                for(int j = 0; j < gameboy->cartridge->getRomBanks(); j++) {
                    applyGGCheatsToBank(j);
                }
            }
        } else {
            unapplyGGCheat(cheat);
            cheats[cheat].flags &= ~CHEAT_FLAG_ENABLED;
        }
    }
}

void CheatEngine::applyGGCheatsToBank(int bank) {
    if(gameboy->cartridge != NULL) {
        u8* bankPtr = gameboy->cartridge->getRomBank(bank);
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
}

void CheatEngine::unapplyGGCheat(int cheat) {
    if(gameboy->cartridge != NULL && (cheats[cheat].flags & CHEAT_FLAG_TYPE_MASK) != CHEAT_FLAG_GAMESHARK) {
        for(unsigned int i = 0; i < cheats[cheat].patchedBanks.size(); i++) {
            u8* bank = gameboy->cartridge->getRomBank(cheats[cheat].patchedBanks[i]);
            bank[cheats[cheat].address & 0x3fff] = (u8) cheats[cheat].patchedValues[i];
        }

        cheats[cheat].patchedBanks = std::vector<int>();
        cheats[cheat].patchedValues = std::vector<int>();
    }
}

void CheatEngine::applyGSCheats() {
    for(int i = 0; i < numCheats; i++) {
        if(cheats[i].flags & CHEAT_FLAG_ENABLED && ((cheats[i].flags & CHEAT_FLAG_TYPE_MASK) == CHEAT_FLAG_GAMESHARK)) {
            u8 oldSvbk = gameboy->mmu->read(SVBK);
            switch(cheats[i].bank & 0xf0) {
                case 0x90:
                    gameboy->mmu->write(SVBK, (u8) (cheats[i].bank & 7));
                    gameboy->mmu->write(cheats[i].address, cheats[i].data);
                    gameboy->mmu->write(SVBK, oldSvbk);
                    break;
                case 0x80: /* TODO : Find info and stuff */
                    break;
                case 0x00:
                    gameboy->mmu->write(cheats[i].address, cheats[i].data);
                    break;
                default:
                    break;
            }
        }
    }
}