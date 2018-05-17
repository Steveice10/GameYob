#include <string.h>

#include <sstream>
#include <vector>

#include "platform/common/menu/keyconfigmenu.h"
#include "platform/common/menu/menu.h"
#include "platform/common/config.h"
#include "platform/input.h"
#include "platform/ui.h"

static int keyConfigChooser_option;
static int keyConfigChooser_cursor;
static int keyConfigChooser_scrollY;

static void redrawKeyConfigChooser() {
    int &option = keyConfigChooser_option;
    int &scrollY = keyConfigChooser_scrollY;
    KeyConfig* config = configGetKeyConfig(configGetSelectedKeyConfig());

    int height = 0;
    uiGetSize(NULL, &height);

    uiClear();

    uiPrint("Config: ");
    if(option == -1) {
        uiSetTextColor(TEXT_COLOR_YELLOW);
        uiPrint("* %s *\n\n", config->name.c_str());
        uiSetTextColor(TEXT_COLOR_NONE);
    } else {
        uiPrint("  %s  \n\n", config->name.c_str());
    }

    uiPrint("              Button   Function\n\n");

    int keyCount = inputGetKeyCount();
    for(int i = 0, elements = 0; i < keyCount && elements < scrollY + height - 7; i++) {
        if(!inputIsValidKey(i)) {
            continue;
        }

        if(elements < scrollY) {
            elements++;
            continue;
        }

        int len = 18 - (int) strlen(inputGetKeyName(i));
        while(len > 0) {
            uiPrint(" ");
            len--;
        }

        if(option == i) {
            uiSetLineHighlighted(true);
            uiPrint("* %s | %s *\n", inputGetKeyName(i), configGetFuncKeyName(config->funcKeys[i]));
            uiSetLineHighlighted(false);
        } else {
            uiPrint("  %s | %s  \n", inputGetKeyName(i), configGetFuncKeyName(config->funcKeys[i]));
        }

        elements++;
    }

    uiPrint("\nPress X to make a new config.");
    if(configGetSelectedKeyConfig() != 0) {
        uiPrint("\nPress Y to delete this config.");
    }

    uiFlush();
}

static void updateKeyConfigChooser() {
    bool redraw = false;

    int &option = keyConfigChooser_option;
    int &cursor = keyConfigChooser_cursor;
    int &scrollY = keyConfigChooser_scrollY;

    KeyConfig* config = configGetKeyConfig(configGetSelectedKeyConfig());
    int keyCount = inputGetKeyCount();

    int height = 0;
    uiGetSize(NULL, &height);

    UIKey key;
    while((key = uiReadKey()) != UI_KEY_NONE) {
        if(key == UI_KEY_B) {
            inputLoadKeyConfig(config);
            closeSubMenu();
        } else if(key == UI_KEY_X) {
            std::stringstream name;
            name << "Custom " << configGetKeyConfigCount();
            configAddKeyConfig(name.str());

            option = -1;
            cursor = -1;
            scrollY = 0;
            redraw = true;
        } else if(key == UI_KEY_Y) {
            u32 selected = configGetSelectedKeyConfig();
            if(selected != 0) /* can't erase the default */ {
                configRemoveKeyConfig(selected);
                redraw = true;
            }
        } else if(key == UI_KEY_DOWN) {
            if(option == keyCount - 1) {
                option = -1;
                cursor = -1;
            } else {
                cursor++;
                option++;
                while(!inputIsValidKey(option)) {
                    option++;
                    if(option >= keyCount) {
                        option = -1;
                        cursor = -1;
                        break;
                    }
                }
            }

            if(cursor < 0) {
                scrollY = 0;
            } else {
                while(cursor < scrollY) {
                    scrollY--;
                }

                while(cursor >= scrollY + height - 7) {
                    scrollY++;
                }
            }

            redraw = true;
        } else if(key == UI_KEY_UP) {
            if(option == -1) {
                option = keyCount - 1;
                while(!inputIsValidKey(option)) {
                    option--;
                    if(option < 0) {
                        option = -1;
                        cursor = -1;
                        break;
                    }
                }

                if(option != -1) {
                    cursor = 0;
                    for(int i = 0; i < keyCount; i++) {
                        if(inputIsValidKey(i)) {
                            cursor++;
                        }
                    }
                }
            } else {
                cursor--;
                option--;
                while(!inputIsValidKey(option)) {
                    option--;
                    if(option < 0) {
                        option = -1;
                        cursor = -1;
                        break;
                    }
                }
            }

            if(cursor < 0) {
                scrollY = 0;
            } else {
                while(cursor < scrollY) {
                    scrollY--;
                }

                while(cursor >= scrollY + height - 7) {
                    scrollY++;
                }
            }

            redraw = true;
        } else if(key == UI_KEY_LEFT) {
            if(option == -1) {
                u32 selected = configGetSelectedKeyConfig();
                if(selected == 0) {
                    configSetSelectedKeyConfig(configGetKeyConfigCount() - 1);
                } else {
                    configSetSelectedKeyConfig(selected - 1);
                }
            } else {
                if(config->funcKeys[option] <= 0) {
                    config->funcKeys[option] = NUM_FUNC_KEYS - 1;
                } else {
                    config->funcKeys[option]--;
                }
            }

            redraw = true;
        } else if(key == UI_KEY_RIGHT) {
            if(option == -1) {
                u32 selected = configGetSelectedKeyConfig();
                if(selected >= configGetKeyConfigCount() - 1) {
                    configSetSelectedKeyConfig(0);
                } else {
                    configSetSelectedKeyConfig(selected + 1);
                }
            } else {
                if(config->funcKeys[option] >= NUM_FUNC_KEYS - 1) {
                    config->funcKeys[option] = FUNC_KEY_NONE;
                } else {
                    config->funcKeys[option]++;
                }
            }

            redraw = true;
        }
    }

    if(redraw) {
        redrawKeyConfigChooser();
    }
}

void startKeyConfigChooser() {
    keyConfigChooser_option = -1;
    keyConfigChooser_cursor = -1;
    keyConfigChooser_scrollY = 0;
    displaySubMenu(updateKeyConfigChooser);
    redrawKeyConfigChooser();
}