#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "platform/input.h"
#include "ui/config.h"
#include "ui/filechooser.h"
#include "ui/menu.h"
#include "gameboy.h"

#include <strings.h>

#define INI_PATH "/gameyob.ini"

char biosPath[256] = "/gbc_bios.bin";
char borderPath[256] = "/default_border.png";
char romPath[256] = "/gb/";

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
        } else if (strcasecmp(parameter, "biosfile") == 0) {
            strcpy(biosPath, value);
        } else if(strcasecmp(parameter, "borderfile") == 0) {
            strcpy(borderPath, value);
        }
    }

    FILE* file = fopen(biosPath, "rb");
    gameboy->biosLoaded = file != NULL;
    if(gameboy->biosLoaded) {
        struct stat st;
        fstat(fileno(file), &st);
        gameboy->gbBios = st.st_size == 0x100;

        fread(gameboy->bios, 1, 0x900, file);
        fclose(file);
    }
}

void generalPrintConfig(FILE* file) {
    fprintf(file, "rompath=%s\n", romPath);
    fprintf(file, "biosfile=%s\n", biosPath);
    fprintf(file, "borderfile=%s\n", borderPath);
}

bool readConfigFile() {
    FILE* file = fopen(INI_PATH, "r");
    char line[100];
    void (*configParser)(char*) = generalParseConfig;

    if(file == NULL) {
        goto end;
    }

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
                } else if(strcasecmp(section, "console") == 0) {
                    configParser = menuParseConfig;
                } else if(strcasecmp(section, "controls") == 0) {
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

    if(gameboy->isRomLoaded()) {
        char nameBuf[256];
        sprintf(nameBuf, "%s.cht", gameboy->getRomFile()->getFileName().c_str());
        gameboy->getCheatEngine()->saveCheats(nameBuf);
    }
}

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
        "Reset",
        "Screenshot"
};

std::vector<KeyConfig> keyConfigs;
unsigned int selectedKeyConfig = 0;

void controlsParseConfig(char* line2) {
    char line[100];
    strncpy(line, line2, 100);
    while(strlen(line) > 0 && (line[strlen(line) - 1] == '\n' || line[strlen(line) - 1] == ' ')) {
        line[strlen(line) - 1] = '\0';
    }

    if(line[0] == '(') {
        char* bracketEnd;
        if((bracketEnd = strrchr(line, ')')) != 0) {
            *bracketEnd = '\0';
            const char* name = line + 1;

            keyConfigs.push_back(KeyConfig());
            KeyConfig* config = &keyConfigs.back();
            strncpy(config->name, name, 32);
            config->name[31] = '\0';
            for(int i = 0; i < NUM_BINDABLE_BUTTONS; i++) {
                if(strlen(inputGetKeyName(i)) > 0) {
                    config->funcKeys[i] = FUNC_KEY_NONE;
                }
            }
        }

        return;
    }

    char* equalsPos;
    if((equalsPos = strrchr(line, '=')) != 0 && equalsPos != line + strlen(line) - 1) {
        *equalsPos = '\0';

        if(strcasecmp(line, "config") == 0) {
            selectedKeyConfig = atoi(equalsPos + 1);
        } else {
            if(strlen(line) > 0) {
                int dsKey = -1;
                for(int i = 0; i < NUM_BINDABLE_BUTTONS; i++) {
                    if(strcasecmp(line, inputGetKeyName(i)) == 0) {
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
}

void controlsCheckConfig() {
    if(keyConfigs.empty()) {
        keyConfigs.push_back(inputGetDefaultKeyConfig());
    }

    if(selectedKeyConfig >= keyConfigs.size()) {
        selectedKeyConfig = 0;
    }

    inputLoadKeyConfig(&keyConfigs[selectedKeyConfig]);
}

void controlsPrintConfig(FILE* file) {
    fprintf(file, "config=%d\n", selectedKeyConfig);
    for(unsigned int i = 0; i < keyConfigs.size(); i++) {
        fprintf(file, "(%s)\n", keyConfigs[i].name);
        for(int j = 0; j < NUM_BINDABLE_BUTTONS; j++) {
            if(strlen(inputGetKeyName(j)) > 0) {
                fprintf(file, "%s=%s\n", inputGetKeyName(j), gbKeyNames[keyConfigs[i].funcKeys[j]]);
            }
        }
    }
}

int keyConfigChooser_option;

void redrawKeyConfigChooser() {
    int &option = keyConfigChooser_option;
    KeyConfig* config = &keyConfigs[selectedKeyConfig];

    iprintf("\x1b[2J");

    printf("Config: ");
    if(option == -1) {
        printf("\x1b[1m\x1b[33m* %s *\n\n\x1b[0m", config->name);
    } else {
        printf("  %s  \n\n", config->name);
    }

    printf("       Button   Function\n\n");

    for(int i = 0; i < NUM_BINDABLE_BUTTONS; i++) {
        if(!inputIsValidKey(i)) {
            continue;
        }

        int len = 11 - strlen(inputGetKeyName(i));
        while(len > 0) {
            printf(" ");
            len--;
        }

        if(option == i) {
            printf("\x1b[1m\x1b[33m* %s | %s *\n\x1b[0m", inputGetKeyName(i), gbKeyNames[config->funcKeys[i]]);
        } else {
            printf("  %s | %s  \n", inputGetKeyName(i), gbKeyNames[config->funcKeys[i]]);
        }
    }

    printf("\nPress X to make a new config.");
    if(selectedKeyConfig != 0) /* can't erase the default */ {
        printf("\n\nPress Y to delete this config.");
    }
}

void updateKeyConfigChooser() {
    bool redraw = false;

    int &option = keyConfigChooser_option;
    KeyConfig* config = &keyConfigs[selectedKeyConfig];

    if(inputKeyPressed(inputMapMenuKey(MENU_KEY_B))) {
        inputLoadKeyConfig(config);
        closeSubMenu();
    } else if(inputKeyPressed(inputMapMenuKey(MENU_KEY_X))) {
        keyConfigs.push_back(KeyConfig(*config));
        selectedKeyConfig = keyConfigs.size() - 1;
        char name[32];
        sprintf(name, "Custom %d", keyConfigs.size() - 1);
        strcpy(keyConfigs.back().name, name);
        option = -1;
        redraw = true;
    } else if(inputKeyPressed(inputMapMenuKey(MENU_KEY_Y))) {
        if(selectedKeyConfig != 0) /* can't erase the default */ {
            keyConfigs.erase(keyConfigs.begin() + selectedKeyConfig);
            if(selectedKeyConfig >= keyConfigs.size()) {
                selectedKeyConfig = keyConfigs.size() - 1;
            }

            redraw = true;
        }
    } else if(inputKeyRepeat(inputMapMenuKey(MENU_KEY_DOWN))) {
        if(option == NUM_BINDABLE_BUTTONS - 1) {
            option = -1;
        } else {
            option++;
            while(!inputIsValidKey(option)) {
                option++;
            }
        }

        redraw = true;
    } else if(inputKeyRepeat(inputMapMenuKey(MENU_KEY_UP))) {
        if(option == -1) {
            option = NUM_BINDABLE_BUTTONS - 1;
        } else {
            option--;
            while(!inputIsValidKey(option)) {
                option--;
            }
        }

        redraw = true;
    } else if(inputKeyRepeat(inputMapMenuKey(MENU_KEY_LEFT))) {
        if(option == -1) {
            if(selectedKeyConfig == 0) {
                selectedKeyConfig = keyConfigs.size() - 1;
            } else {
                selectedKeyConfig--;
            }
        } else {
            config->funcKeys[option]--;
            if(config->funcKeys[option] < 0) {
                config->funcKeys[option] = NUM_FUNC_KEYS - 1;
            }
        }

        redraw = true;
    } else if(inputKeyRepeat(inputMapMenuKey(MENU_KEY_RIGHT))) {
        if(option == -1) {
            selectedKeyConfig++;
            if(selectedKeyConfig >= keyConfigs.size()) {
                selectedKeyConfig = 0;
            }
        } else {
            config->funcKeys[option]++;
            if(config->funcKeys[option] >= NUM_FUNC_KEYS) {
                config->funcKeys[option] = 0;
            }
        }

        redraw = true;
    }

    if(redraw) {
        redrawKeyConfigChooser();
    }
}

void startKeyConfigChooser() {
    keyConfigChooser_option = -1;
    displaySubMenu(updateKeyConfigChooser);
    redrawKeyConfigChooser();
}
