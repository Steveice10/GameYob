#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <string.h>
#include <stdlib.h>
#include "io.h"
#include "config.h"
#include "inputhelper.h"
#include "gbgfx.h"
#include "menu.h"
#include "console.h"
#include "filechooser.h"
#include "gameboy.h"
#include "cheats.h"
#include "romfile.h"

#ifdef SDL
#define INI_PATH "gameyob.ini"
#else
#define INI_PATH "/gameyob.ini"
#endif

char borderPath[MAX_FILENAME_LEN] = "";
char romPath[MAX_FILENAME_LEN] = "";

void controlsParseConfig(char* line);
void controlsPrintConfig(FileHandle* f);
void controlsCheckConfig();

void generalParseConfig(char* line) {
    char* equalsPos;
    if ((equalsPos = strrchr(line, '=')) != 0 && equalsPos != line+strlen(line)-1) {
        *equalsPos = '\0';
        const char* parameter = line;
        const char* value = equalsPos+1;

        if (strcasecmp(parameter, "rompath") == 0) {
            strcpy(romPath, value);
            romChooserState.directory = romPath;
        }
        else if (strcasecmp(parameter, "borderfile") == 0) {
            strcpy(borderPath, value);
        }
    }
    if (*borderPath == '\0') {
        strcpy(borderPath, "/border.bmp");
    }
}

void generalPrintConfig(FileHandle* file) {
        file_printf(file, "rompath=%s\n", romPath);
        file_printf(file, "borderfile=%s\n", borderPath);
}

bool readConfigFile() {
    FileHandle* file = file_open(INI_PATH, "r");
    char line[100];
    void (*configParser)(char*) = generalParseConfig;

    if (file == NULL)
        goto end;

    while (file_tell(file) < file_getSize(file)) {
        file_gets(line, 100, file);
        char c=0;
        while (*line != '\0' && ((c = line[strlen(line)-1]) == ' ' || c == '\n' || c == '\r'))
            line[strlen(line)-1] = '\0';
        if (line[0] == '[') {
            char* endBrace;
            if ((endBrace = strrchr(line, ']')) != 0) {
                *endBrace = '\0';
                const char* section = line+1;
                if (strcasecmp(section, "general") == 0) {
                    configParser = generalParseConfig;
                }
                else if (strcasecmp(section, "console") == 0) {
                    configParser = menuParseConfig;
                }
                else if (strcasecmp(section, "controls") == 0) {
                    configParser = controlsParseConfig;
                }
            }
        }
        else
            configParser(line);
    }
    file_close(file);
end:
    controlsCheckConfig();

    return file != NULL;
}

void writeConfigFile() {
    FileHandle* file = file_open(INI_PATH, "w");
    if (file == NULL) {
        printMenuMessage("Error opening gameyob.ini.");
        return;
    }

    file_printf(file, "[general]\n");
    generalPrintConfig(file);
    file_printf(file, "[console]\n");
    menuPrintConfig(file);
    file_printf(file, "[controls]\n");
    controlsPrintConfig(file);
    file_close(file);

    char nameBuf[MAX_FILENAME_LEN];
    sprintf(nameBuf, "%s.cht", gameboy->getRomFile()->getBasename());
    gameboy->getCheatEngine()->saveCheats(nameBuf);
}


// Keys: DS and 3DS specific code

#if defined(DS) || defined(_3DS)

#if defined(DS)
#include <nds.h>
#elif defined(_3DS)
#include <3ds.h>
#endif

const char* gbKeyNames[] = {
        "-",
        "A",
        "B",
        "Left",
        "Right",
        "Up",
        "Down",
        "Start",
        "Select",
        "Menu",
        "Menu/Pause",
        "Save",
        "Autofire A",
        "Autofire B",
        "Fast Forward",
        "FF Toggle",
        "Scale","Reset"
};

const char* dsKeyNames[] = {
        "A",         // 0
        "B",         // 1
        "Select",    // 2
        "Start",     // 3
        "Right",     // 4
        "Left",      // 5
        "Up",        // 6
        "Down",      // 7
        "R",         // 8
        "L",         // 9
        "X",         // 10
        "Y",         // 11
        "",          // 12
        "",          // 13
        "ZL",        // 14
        "ZR",        // 15
        "",          // 16
        "",          // 17
        "",          // 18
        "",          // 19
        "",          // 20
        "",          // 21
        "",          // 22
        "",          // 23
        "C-Right",   // 24
        "C-Left",    // 25
        "C-Up",      // 26
        "C-Down",    // 27
        "Pad-Right", // 28
        "Pad-Left",  // 29
        "Pad-Up",    // 30
        "Pad-Down"   // 31
};

int keyMapping[NUM_FUNC_KEYS];
#if defined(DS)
#define NUM_BINDABLE_BUTTONS 12
struct KeyConfig {
    char name[32];
    int funcKeys[12];
};
#elif defined(_3DS)
#define NUM_BINDABLE_BUTTONS 32
struct KeyConfig {
    char name[32];
    int funcKeys[32];
};
#endif

#if defined(DS)
KeyConfig defaultKeyConfig = {
    "Main",
    {
            FUNC_KEY_A,            // 0 = KEY_A
            FUNC_KEY_B,            // 1 = KEY_B
            FUNC_KEY_SELECT,       // 2 = KEY_SELECT
            FUNC_KEY_START,        // 3 = KEY_START
            FUNC_KEY_RIGHT,        // 4 = KEY_RIGHT
            FUNC_KEY_LEFT,         // 5 = KEY_LEFT
            FUNC_KEY_UP,           // 6 = KEY_UP
            FUNC_KEY_DOWN,         // 7 = KEY_DOWN
            FUNC_KEY_MENU,         // 8 = KEY_R
            FUNC_KEY_FAST_FORWARD, // 9 = KEY_L
            FUNC_KEY_START,        // 10 = KEY_X
            FUNC_KEY_SELECT        // 11 = KEY_Y
    }
};
#elif defined(_3DS)
KeyConfig defaultKeyConfig = {
    "Main",
    {
            FUNC_KEY_A,            // 0 = KEY_A
            FUNC_KEY_B,            // 1 = KEY_B
            FUNC_KEY_SELECT,       // 2 = KEY_SELECT
            FUNC_KEY_START,        // 3 = KEY_START
            FUNC_KEY_RIGHT,        // 4 = KEY_DRIGHT
            FUNC_KEY_LEFT,         // 5 = KEY_DLEFT
            FUNC_KEY_UP,           // 6 = KEY_DUP
            FUNC_KEY_DOWN,         // 7 = KEY_DDOWN
            FUNC_KEY_MENU,         // 8 = KEY_R
            FUNC_KEY_FAST_FORWARD, // 9 = KEY_L
            FUNC_KEY_START,        // 10 = KEY_X
            FUNC_KEY_SELECT,       // 11 = KEY_Y
            FUNC_KEY_NONE,         // 12 = KEY_NONE
            FUNC_KEY_NONE,         // 13 = KEY_NONE
            FUNC_KEY_NONE,         // 14 = KEY_ZL
            FUNC_KEY_NONE,         // 15 = KEY_ZR
            FUNC_KEY_NONE,         // 16 = KEY_NONE
            FUNC_KEY_NONE,         // 17 = KEY_NONE
            FUNC_KEY_NONE,         // 18 = KEY_NONE
            FUNC_KEY_NONE,         // 19 = KEY_NONE
            FUNC_KEY_NONE,         // 20 = KEY_TOUCH
            FUNC_KEY_NONE,         // 21 = KEY_NONE
            FUNC_KEY_NONE,         // 22 = KEY_NONE
            FUNC_KEY_NONE,         // 23 = KEY_NONE
            FUNC_KEY_RIGHT,        // 24 = KEY_CSTICK_RIGHT
            FUNC_KEY_LEFT,         // 25 = KEY_CSTICK_LEFT
            FUNC_KEY_UP,           // 26 = KEY_CSTICK_UP
            FUNC_KEY_DOWN,         // 27 = KEY_CSTICK_DOWN
            FUNC_KEY_RIGHT,        // 28 = KEY_CPAD_RIGHT
            FUNC_KEY_LEFT,         // 29 = KEY_CPAD_LEFT
            FUNC_KEY_UP,           // 30 = KEY_CPAD_UP
            FUNC_KEY_DOWN          // 31 = KEY_CPAD_DOWN
    }
};
#endif


std::vector<KeyConfig> keyConfigs;
unsigned int selectedKeyConfig=0;

void loadKeyConfig() {
    KeyConfig* keyConfig = &keyConfigs[selectedKeyConfig];
    for (int i=0; i<NUM_FUNC_KEYS; i++)
        keyMapping[i] = 0;
    for (int i=0; i<NUM_BINDABLE_BUTTONS; i++) {
        keyMapping[keyConfig->funcKeys[i]] |= BIT(i);
    }
}

void controlsParseConfig(char* line2) {
    char line[100];
    strncpy(line, line2, 100);
    while (strlen(line) > 0 && (line[strlen(line)-1] == '\n' || line[strlen(line)-1] == ' '))
        line[strlen(line)-1] = '\0';
    if (line[0] == '(') {
        char* bracketEnd;
        if ((bracketEnd = strrchr(line, ')')) != 0) {
            *bracketEnd = '\0';
            const char* name = line+1;

            keyConfigs.push_back(KeyConfig());
            KeyConfig* config = &keyConfigs.back();
            strncpy(config->name, name, 32);
            config->name[31] = '\0';
            for (int i=0; i<NUM_BINDABLE_BUTTONS; i++)
                config->funcKeys[i] = FUNC_KEY_NONE;
        }
        return;
    }
    char* equalsPos;
    if ((equalsPos = strrchr(line, '=')) != 0 && equalsPos != line+strlen(line)-1) {
        *equalsPos = '\0';

        if (strcasecmp(line, "config") == 0) {
            selectedKeyConfig = atoi(equalsPos+1);
        }
        else {
            int dsKey = -1;
            for (int i=0; i<NUM_BINDABLE_BUTTONS; i++) {
                if (strcasecmp(line, dsKeyNames[i]) == 0) {
                    dsKey = i;
                    break;
                }
            }
            int gbKey = -1;
            for (int i=0; i<NUM_FUNC_KEYS; i++) {
                if (strcasecmp(equalsPos+1, gbKeyNames[i]) == 0) {
                    gbKey = i;
                    break;
                }
            }

            if (gbKey != -1 && dsKey != -1) {
                KeyConfig* config = &keyConfigs.back();
                config->funcKeys[dsKey] = gbKey;
            }
        }
    }
}

void controlsCheckConfig() {
    if (keyConfigs.empty())
        keyConfigs.push_back(defaultKeyConfig);
    if (selectedKeyConfig >= keyConfigs.size())
        selectedKeyConfig = 0;
    loadKeyConfig();
}

void controlsPrintConfig(FileHandle* file) {
    file_printf(file, "config=%d\n", selectedKeyConfig);
    for (unsigned int i=0; i<keyConfigs.size(); i++) {
        file_printf(file, "(%s)\n", keyConfigs[i].name);
        for (int j=0; j<NUM_BINDABLE_BUTTONS; j++) {
            file_printf(file, "%s=%s\n", dsKeyNames[j], gbKeyNames[keyConfigs[i].funcKeys[j]]);
        }
    }
}

int keyConfigChooser_option;

void redrawKeyConfigChooser() {
    int& option = keyConfigChooser_option;
    KeyConfig* config = &keyConfigs[selectedKeyConfig];

    clearConsole();

    iprintf("Config: ");
    if (option == -1)
        iprintfColored(CONSOLE_COLOR_LIGHT_YELLOW, "* %s *\n\n", config->name);
    else
        iprintf("  %s  \n\n", config->name);

    iprintf("       Button   Function\n\n");

    for (int i=0; i<NUM_BINDABLE_BUTTONS; i++) {
#if defined(_3DS)
       // These button bits aren't assigned to anything, so no strings for them
       if((i > 15 && i < 24) || i == 12 || i == 13)
            continue;
#endif

        int len = 11-strlen(dsKeyNames[i]);
        while (len > 0) {
            iprintf(" ");
            len--;
        }
        if (option == i) 
            iprintfColored(CONSOLE_COLOR_LIGHT_YELLOW, "* %s | %s *\n", dsKeyNames[i], gbKeyNames[config->funcKeys[i]]);
        else
            iprintf("  %s | %s  \n", dsKeyNames[i], gbKeyNames[config->funcKeys[i]]);
    }
    iprintf("\nPress X to make a new config.");
    if (selectedKeyConfig != 0) /* can't erase the default */ {
        iprintf("\n\nPress Y to delete this config.");
    }
}

void updateKeyConfigChooser() {
    bool redraw = false;

    int& option = keyConfigChooser_option;
    KeyConfig* config = &keyConfigs[selectedKeyConfig];

    if (keyJustPressed(KEY_B)) {
        loadKeyConfig();
        closeSubMenu();
    }
    else if (keyJustPressed(KEY_X)) {
        keyConfigs.push_back(KeyConfig(*config));
        selectedKeyConfig = keyConfigs.size()-1;
        char name[32];
        sprintf(name, "Custom %d", keyConfigs.size()-1);
        strcpy(keyConfigs.back().name, name);
        option = -1;
        redraw = true;
    }
    else if (keyJustPressed(KEY_Y)) {
        if (selectedKeyConfig != 0) /* can't erase the default */ {
            keyConfigs.erase(keyConfigs.begin() + selectedKeyConfig);
            if (selectedKeyConfig >= keyConfigs.size())
                selectedKeyConfig = keyConfigs.size() - 1;
            redraw = true;
        }
    }
    else if (keyPressedAutoRepeat(KEY_DOWN)) {
        if (option == NUM_BINDABLE_BUTTONS-1)
            option = -1;
#if defined(_3DS)
        else if(option == 11) //Skip nonexistant keys
            option = 14;
        else if(option == 15)
            option = 24;
#endif
        else
            option++;
        redraw = true;
    }
    else if (keyPressedAutoRepeat(KEY_UP)) {
        if (option == -1)
            option = NUM_BINDABLE_BUTTONS-1;
#if defined(_3DS)
        else if(option == 14) //Skip nonexistant keys
            option = 11;
        else if(option == 24)
            option = 15;
#endif
        else
            option--;
        redraw = true;
    }
    else if (keyPressedAutoRepeat(KEY_LEFT)) {
        if (option == -1) {
            if (selectedKeyConfig == 0)
                selectedKeyConfig = keyConfigs.size()-1;
            else
                selectedKeyConfig--;
        }
        else {
            config->funcKeys[option]--;
            if (config->funcKeys[option] < 0)
                config->funcKeys[option] = NUM_FUNC_KEYS-1;
        }
        redraw = true;
    }
    else if (keyPressedAutoRepeat(KEY_RIGHT)) {
        if (option == -1) {
            selectedKeyConfig++;
            if (selectedKeyConfig >= keyConfigs.size())
                selectedKeyConfig = 0;
        }
        else {
            config->funcKeys[option]++;
            if (config->funcKeys[option] >= NUM_FUNC_KEYS)
                config->funcKeys[option] = 0;
        }
        redraw = true;
    }
    if (redraw)
        doAtVBlank(redrawKeyConfigChooser);
}

void startKeyConfigChooser() {
    keyConfigChooser_option = -1;
    displaySubMenu(updateKeyConfigChooser);
    redrawKeyConfigChooser();
}

int mapFuncKey(int funcKey) {
    return keyMapping[funcKey];
}

int mapMenuKey(int menuKey) {
    switch (menuKey) {
        case MENU_KEY_A:
            return KEY_A;
        case MENU_KEY_B:
            return KEY_B;
        case MENU_KEY_LEFT:
            return KEY_LEFT;
        case MENU_KEY_RIGHT:
            return KEY_RIGHT;
        case MENU_KEY_UP:
            return KEY_UP;
        case MENU_KEY_DOWN:
            return KEY_DOWN;
        case MENU_KEY_R:
            return KEY_R;
        case MENU_KEY_L:
            return KEY_L;
        case MENU_KEY_X:
            return KEY_X;
        case MENU_KEY_Y:
            return KEY_Y;
    }
    return 0;
}

#elif defined(SDL)

#include <SDL.h>

// Stub functions for SDL

void controlsParseConfig(char* line2) {
}
void controlsPrintConfig(FileHandle* file) {
}
void controlsCheckConfig() {
}

void startKeyConfigChooser() {
}

int mapFuncKey(int funcKey) {
    switch(funcKey) {
        case FUNC_KEY_NONE:
            return 0;
        case FUNC_KEY_A:
            return SDLK_SEMICOLON;
        case FUNC_KEY_B:
            return SDLK_q;
        case FUNC_KEY_LEFT:
            return SDLK_LEFT;
        case FUNC_KEY_RIGHT:
            return SDLK_RIGHT;
        case FUNC_KEY_UP:
            return SDLK_UP;
        case FUNC_KEY_DOWN:
            return SDLK_DOWN;
        case FUNC_KEY_START:
            return SDLK_RETURN;
        case FUNC_KEY_SELECT:
            return SDLK_BACKSLASH;
        case FUNC_KEY_MENU:
            return SDLK_o;
        case FUNC_KEY_MENU_PAUSE:
            return 0;
        case FUNC_KEY_SAVE:
            return 0;
        case FUNC_KEY_AUTO_A:
            return 0;
        case FUNC_KEY_AUTO_B:
            return 0;
        case FUNC_KEY_FAST_FORWARD:
            return SDLK_SPACE;
        case FUNC_KEY_FAST_FORWARD_TOGGLE:
            return 0;
        case FUNC_KEY_SCALE:
            return 0;
        case FUNC_KEY_RESET:
            return 0;
    }
    return 0;
}

int mapMenuKey(int menuKey) {
    switch (menuKey) {
        case MENU_KEY_A:
            return SDLK_SEMICOLON;
        case MENU_KEY_B:
            return SDLK_q;
        case MENU_KEY_UP:
            return SDLK_UP;
        case MENU_KEY_DOWN:
            return SDLK_DOWN;
        case MENU_KEY_LEFT:
            return SDLK_LEFT;
        case MENU_KEY_RIGHT:
            return SDLK_RIGHT;
        case MENU_KEY_L:
            return SDLK_a;
        case MENU_KEY_R:
            return SDLK_o;
    }
    return 0;
}

#endif
