#pragma once

#include "types.h"

#include <vector>

#define MAX_CHEATS            900
#define CHEAT_FLAG_ENABLED    (1<<0)
#define CHEAT_FLAG_TYPE_MASK  (3<<2)
#define CHEAT_FLAG_GAMEGENIE  (1<<2)
#define CHEAT_FLAG_GAMEGENIE1 (2<<2)
#define CHEAT_FLAG_GAMESHARK  (3<<2)

typedef struct cheat_t {
    std::string name;
    std::string cheatString;
    u8 flags;
    u8 data;
    u16 address;
    union {
        u8 compare; /* For GameGenie codes */
        u8 bank;    /* For Gameshark codes */
    };
    std::vector<int> patchedBanks;  /* For GameGenie codes */
    std::vector<int> patchedValues; /* For GameGenie codes */
} cheat_t;

class Gameboy;

class CheatEngine {
public:
    CheatEngine(Gameboy* g);

    void loadCheats(const std::string& filename);
    void saveCheats(const std::string& filename);

    inline int getNumCheats() { return numCheats; }

    inline const std::string& getCheatName(int cheat) { return cheats[cheat].name; }
    inline bool isCheatEnabled(int cheat) { return (cheats[cheat].flags & CHEAT_FLAG_ENABLED) != 0; }

    bool addCheat(const std::string& name, const std::string& value);
    void toggleCheat(int cheat, bool enabled);

    void applyGGCheatsToBank(int romBank);
    void unapplyGGCheat(int cheat);

    void applyGSCheats();
private:
    cheat_t cheats[MAX_CHEATS];
    int numCheats;

    Gameboy* gameboy;

    static int parseCheats(void* user, const char* section, const char* name, const char* value);
};