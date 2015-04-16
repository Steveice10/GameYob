#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vector>

#include "config.h"
#include "inputhelper.h"
#include "gbgfx.h"
#include "menu.h"
#include "filechooser.h"
#include "gameboy.h"
#include "cheats.h"

#include <ctrcommon/input.hpp>

#define INI_PATH "/gameyob.ini"

char borderPath[256] = "";
char romPath[256] = "";

void controlsParseConfig(char* line);
void controlsPrintConfig(FILE* f);
void controlsCheckConfig();

void generalParseConfig(char* line) {
    char* equalsPos;
    if((equalsPos = strrchr(line, '=')) != 0 && equalsPos != line + strlen(line) - 1) {
        *equalsPos = '\0';
        const char* parameter = line;
        const char* value = equalsPos + 1;

        if(strcasecmp(parameter, "rompath") == 0) {
            strcpy(romPath, value);
            romChooserState.directory = romPath;
        }
        else if(strcasecmp(parameter, "borderfile") == 0) {
            strcpy(borderPath, value);
        }
    }
    if(*borderPath == '\0') {
        strcpy(borderPath, "/border.bmp");
    }
}

void generalPrintConfig(FILE* file) {
    fprintf(file, "rompath=%s\n", romPath);
    fprintf(file, "borderfile=%s\n", borderPath);
}

bool readConfigFile() {
    FILE* file = fopen(INI_PATH, "r");
    char line[100];
    void (* configParser)(char*) = generalParseConfig;

    if(file == NULL)
        goto end;

    struct stat s;
    fstat(fileno(file), &s);
    while(ftell(file) < s.st_size) {
        fgets(line, 100, file);
        char c = 0;
        while(*line != '\0' && ((c = line[strlen(line) - 1]) == ' ' || c == '\n' || c == '\r')) {
            line[strlen(line) - 1] = '\0';
        }

        if(line[0] == '[') {
            char* endBrace;
            if((endBrace = strrchr(line, ']')) != 0) {
                *endBrace = '\0';
                const char* section = line + 1;
                if(strcasecmp(section, "general") == 0) {
                    configParser = generalParseConfig;
                }
                else if(strcasecmp(section, "console") == 0) {
                    configParser = menuParseConfig;
                }
                else if(strcasecmp(section, "controls") == 0) {
                    configParser = controlsParseConfig;
                }
            }
        } else {
            configParser(line);
        }
    }
    fclose(file);
    end:
    controlsCheckConfig();

    return file != NULL;
}

void writeConfigFile() {
    FILE* file = fopen(INI_PATH, "w");
    if(file == NULL) {
        printMenuMessage("Error opening gameyob.ini.");
        return;
    }

    fprintf(file, "[general]\n");
    generalPrintConfig(file);
    fprintf(file, "[console]\n");
    menuPrintConfig(file);
    fprintf(file, "[controls]\n");
    controlsPrintConfig(file);
    fclose(file);

    char nameBuf[256];
    sprintf(nameBuf, "%s.cht", gameboy->getRomFile()->getBasename());
    gameboy->getCheatEngine()->saveCheats(nameBuf);
}


// Keys: 3DS specific code

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
        "Scale",
        "Reset"
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
#define NUM_BINDABLE_BUTTONS 32
struct KeyConfig {
    char name[32];
    int funcKeys[32];
};

KeyConfig defaultKeyConfig = {
        "Main",
        {
                FUNC_KEY_A,                   // 0 = BUTTON_A
                FUNC_KEY_B,                   // 1 = BUTTON_B
                FUNC_KEY_SELECT,              // 2 = BUTTON_SELECT
                FUNC_KEY_START,               // 3 = BUTTON_START
                FUNC_KEY_RIGHT,               // 4 = BUTTON_DRIGHT
                FUNC_KEY_LEFT,                // 5 = BUTTON_DLEFT
                FUNC_KEY_UP,                  // 6 = BUTTON_DUP
                FUNC_KEY_DOWN,                // 7 = BUTTON_DDOWN
                FUNC_KEY_MENU,                // 8 = BUTTON_R
                FUNC_KEY_FAST_FORWARD,        // 9 = BUTTON_L
                FUNC_KEY_START,               // 10 = BUTTON_X
                FUNC_KEY_SELECT,              // 11 = BUTTON_Y
                FUNC_KEY_NONE,                // 12 = BUTTON_NONE
                FUNC_KEY_NONE,                // 13 = BUTTON_NONE
                FUNC_KEY_FAST_FORWARD_TOGGLE, // 14 = BUTTON_ZL
                FUNC_KEY_SCALE,               // 15 = BUTTON_ZR
                FUNC_KEY_NONE,                // 16 = BUTTON_NONE
                FUNC_KEY_NONE,                // 17 = BUTTON_NONE
                FUNC_KEY_NONE,                // 18 = BUTTON_NONE
                FUNC_KEY_NONE,                // 19 = BUTTON_NONE
                FUNC_KEY_NONE,                // 20 = BUTTON_TOUCH
                FUNC_KEY_NONE,                // 21 = BUTTON_NONE
                FUNC_KEY_NONE,                // 22 = BUTTON_NONE
                FUNC_KEY_NONE,                // 23 = BUTTON_NONE
                FUNC_KEY_RIGHT,               // 24 = BUTTON_CSTICK_RIGHT
                FUNC_KEY_LEFT,                // 25 = BUTTON_CSTICK_LEFT
                FUNC_KEY_UP,                  // 26 = BUTTON_CSTICK_UP
                FUNC_KEY_DOWN,                // 27 = BUTTON_CSTICK_DOWN
                FUNC_KEY_RIGHT,               // 28 = BUTTON_CPAD_RIGHT
                FUNC_KEY_LEFT,                // 29 = BUTTON_CPAD_LEFT
                FUNC_KEY_UP,                  // 30 = BUTTON_CPAD_UP
                FUNC_KEY_DOWN                 // 31 = BUTTON_CPAD_DOWN
        }
};

std::vector<KeyConfig> keyConfigs;
unsigned int selectedKeyConfig = 0;

void loadKeyConfig() {
    KeyConfig* keyConfig = &keyConfigs[selectedKeyConfig];
    memset(keyMapping, 0, NUM_FUNC_KEYS * sizeof(int));
    for(int i = 0; i < NUM_BINDABLE_BUTTONS; i++) {
        keyMapping[keyConfig->funcKeys[i]] |= (1 << i);
    }
}

void controlsParseConfig(char* line2) {
    char line[100];
    strncpy(line, line2, 100);
    while(strlen(line) > 0 && (line[strlen(line) - 1] == '\n' || line[strlen(line) - 1] == ' '))
        line[strlen(line) - 1] = '\0';
    if(line[0] == '(') {
        char* bracketEnd;
        if((bracketEnd = strrchr(line, ')')) != 0) {
            *bracketEnd = '\0';
            const char* name = line + 1;

            keyConfigs.push_back(KeyConfig());
            KeyConfig* config = &keyConfigs.back();
            strncpy(config->name, name, 32);
            config->name[31] = '\0';
            for(int i = 0; i < NUM_BINDABLE_BUTTONS; i++)
                config->funcKeys[i] = FUNC_KEY_NONE;
        }
        return;
    }
    char* equalsPos;
    if((equalsPos = strrchr(line, '=')) != 0 && equalsPos != line + strlen(line) - 1) {
        *equalsPos = '\0';

        if(strcasecmp(line, "config") == 0) {
            selectedKeyConfig = atoi(equalsPos + 1);
        }
        else {
            int dsKey = -1;
            for(int i = 0; i < NUM_BINDABLE_BUTTONS; i++) {
                if(strcasecmp(line, dsKeyNames[i]) == 0) {
                    dsKey = i;
                    break;
                }
            }
            int gbKey = -1;
            for(int i = 0; i < NUM_FUNC_KEYS; i++) {
                if(strcasecmp(equalsPos + 1, gbKeyNames[i]) == 0) {
                    gbKey = i;
                    break;
                }
            }

            if(gbKey != -1 && dsKey != -1) {
                KeyConfig* config = &keyConfigs.back();
                config->funcKeys[dsKey] = gbKey;
            }
        }
    }
}

void controlsCheckConfig() {
    if(keyConfigs.empty())
        keyConfigs.push_back(defaultKeyConfig);
    if(selectedKeyConfig >= keyConfigs.size())
        selectedKeyConfig = 0;
    loadKeyConfig();
}

void controlsPrintConfig(FILE* file) {
    fprintf(file, "config=%d\n", selectedKeyConfig);
    for(unsigned int i = 0; i < keyConfigs.size(); i++) {
        fprintf(file, "(%s)\n", keyConfigs[i].name);
        for(int j = 0; j < NUM_BINDABLE_BUTTONS; j++) {
            fprintf(file, "%s=%s\n", dsKeyNames[j], gbKeyNames[keyConfigs[i].funcKeys[j]]);
        }
    }
}

int keyConfigChooser_option;

void redrawKeyConfigChooser() {
    int &option = keyConfigChooser_option;
    KeyConfig* config = &keyConfigs[selectedKeyConfig];

    clearConsole();

    iprintf("Config: ");
    if(option == -1)
        iprintfColored(CONSOLE_COLOR_BRIGHT_YELLOW, "* %s *\n\n", config->name);
    else
        iprintf("  %s  \n\n", config->name);

    iprintf("       Button   Function\n\n");

    for(int i = 0; i < NUM_BINDABLE_BUTTONS; i++) {
        // These button bits aren't assigned to anything, so no strings for them
        if((i > 15 && i < 24) || i == 12 || i == 13)
            continue;

        int len = 11 - strlen(dsKeyNames[i]);
        while(len > 0) {
            iprintf(" ");
            len--;
        }
        if(option == i)
            iprintfColored(CONSOLE_COLOR_BRIGHT_YELLOW, "* %s | %s *\n", dsKeyNames[i],
                           gbKeyNames[config->funcKeys[i]]);
        else
            iprintf("  %s | %s  \n", dsKeyNames[i], gbKeyNames[config->funcKeys[i]]);
    }
    iprintf("\nPress X to make a new config.");
    if(selectedKeyConfig != 0) /* can't erase the default */ {
        iprintf("\n\nPress Y to delete this config.");
    }
}

void updateKeyConfigChooser() {
    bool redraw = false;

    int &option = keyConfigChooser_option;
    KeyConfig* config = &keyConfigs[selectedKeyConfig];

    if(keyJustPressed(BUTTON_B)) {
        loadKeyConfig();
        closeSubMenu();
    }
    else if(keyJustPressed(BUTTON_X)) {
        keyConfigs.push_back(KeyConfig(*config));
        selectedKeyConfig = keyConfigs.size() - 1;
        char name[32];
        sprintf(name, "Custom %d", keyConfigs.size() - 1);
        strcpy(keyConfigs.back().name, name);
        option = -1;
        redraw = true;
    }
    else if(keyJustPressed(BUTTON_Y)) {
        if(selectedKeyConfig != 0) /* can't erase the default */ {
            keyConfigs.erase(keyConfigs.begin() + selectedKeyConfig);
            if(selectedKeyConfig >= keyConfigs.size())
                selectedKeyConfig = keyConfigs.size() - 1;
            redraw = true;
        }
    }
    else if(keyPressedAutoRepeat(BUTTON_DOWN)) {
        if(option == NUM_BINDABLE_BUTTONS - 1)
            option = -1;
        else if(option == 11) //Skip nonexistant keys
            option = 14;
        else if(option == 15)
            option = 24;
        else
            option++;
        redraw = true;
    }
    else if(keyPressedAutoRepeat(BUTTON_UP)) {
        if(option == -1)
            option = NUM_BINDABLE_BUTTONS - 1;
        else if(option == 14) //Skip nonexistant keys
            option = 11;
        else if(option == 24)
            option = 15;
        else
            option--;
        redraw = true;
    }
    else if(keyPressedAutoRepeat(BUTTON_LEFT)) {
        if(option == -1) {
            if(selectedKeyConfig == 0)
                selectedKeyConfig = keyConfigs.size() - 1;
            else
                selectedKeyConfig--;
        }
        else {
            config->funcKeys[option]--;
            if(config->funcKeys[option] < 0)
                config->funcKeys[option] = NUM_FUNC_KEYS - 1;
        }
        redraw = true;
    }
    else if(keyPressedAutoRepeat(BUTTON_RIGHT)) {
        if(option == -1) {
            selectedKeyConfig++;
            if(selectedKeyConfig >= keyConfigs.size())
                selectedKeyConfig = 0;
        }
        else {
            config->funcKeys[option]++;
            if(config->funcKeys[option] >= NUM_FUNC_KEYS)
                config->funcKeys[option] = 0;
        }
        redraw = true;
    }
    if(redraw)
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
    switch(menuKey) {
        case MENU_KEY_A:
            return BUTTON_A;
        case MENU_KEY_B:
            return BUTTON_B;
        case MENU_KEY_LEFT:
            return BUTTON_LEFT;
        case MENU_KEY_RIGHT:
            return BUTTON_RIGHT;
        case MENU_KEY_UP:
            return BUTTON_UP;
        case MENU_KEY_DOWN:
            return BUTTON_DOWN;
        case MENU_KEY_R:
            return BUTTON_R;
        case MENU_KEY_L:
            return BUTTON_L;
        case MENU_KEY_X:
            return BUTTON_X;
        case MENU_KEY_Y:
            return BUTTON_Y;
    }
    return 0;
}
