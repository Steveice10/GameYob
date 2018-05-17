#include <string.h>

#include <fstream>
#include <vector>

#include "libs/inih/INIReader.h"

#include "platform/common/cheatengine.h"
#include "platform/common/config.h"
#include "platform/common/manager.h"
#include "platform/common/menu/menu.h"
#include "platform/input.h"
#include "platform/system.h"
#include "gameboy.h"

static const char* funcKeyNames[NUM_FUNC_KEYS] = {
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

static std::string borderPath = "";
static std::string romPath = "";

static std::vector<KeyConfig> keyConfigs;
static u32 selectedKeyConfig = 0;

static void configLoadGeneral(INIReader &reader) {
    romPath = reader.Get("general", "rompath", "/");
    borderPath = reader.Get("general", "borderfile", "");

    size_t len = romPath.length();
    if(len == 0 || romPath[len - 1] != '/') {
        romPath += "/";
    }
}

static void configSaveGeneral(std::ofstream &stream) {
    stream << "[general]\n";
    stream << "rompath=" << romPath << "\n";
    stream << "borderfile=" << borderPath << "\n";
    stream << "\n";
}

static void configValidateControls() {
    if(keyConfigs.empty()) {
        keyConfigs.push_back(inputGetDefaultKeyConfig());
    }

    if(selectedKeyConfig >= keyConfigs.size()) {
        selectedKeyConfig = (u32) (keyConfigs.size() - 1);
    }

    inputLoadKeyConfig(&keyConfigs[selectedKeyConfig]);
}

static void configLoadControlsProfile(INIReader &reader, const std::string &name) {
    if(name.empty()) {
        return;
    }

    keyConfigs.push_back(KeyConfig());
    KeyConfig& config = keyConfigs.back();

    config.name = name;

    u32 keyCount = (u32) inputGetKeyCount();
    for(u32 i = 0; i < keyCount; i++) {
        if(inputIsValidKey(i)) {
            std::string mappedKey = reader.Get(name, std::string(inputGetKeyName(i)), "");

            if(!mappedKey.empty()) {
                for(u8 funcKey = 0; funcKey < NUM_FUNC_KEYS; funcKey++) {
                    if(strcasecmp(mappedKey.c_str(), funcKeyNames[funcKey]) == 0) {
                        config.funcKeys[i] = funcKey;
                        break;
                    }
                }
            } else {
                config.funcKeys[i] = FUNC_KEY_NONE;
            }
        }
    }
}

static void configLoadControls(INIReader &reader) {
    selectedKeyConfig = (u32) reader.GetInteger("controls", "selected", 0);

    std::string profiles = reader.Get("controls", "profiles", "");
    if(profiles.length() > 0) {
        size_t startPos = 0;
        size_t endPos = 0;
        while((endPos = profiles.find(',', startPos)) != std::string::npos) {
            configLoadControlsProfile(reader, profiles.substr(startPos, endPos));

            startPos = endPos + 1;
        }

        if(startPos < profiles.length()) {
            configLoadControlsProfile(reader, profiles.substr(startPos));
        }
    }

    configValidateControls();
}

static void configSaveControls(std::ofstream &stream) {
    stream << "[controls]\n";
    stream << "selected=" << selectedKeyConfig << "\n";
    stream << "profiles=";
    for(u32 i = 0; i < keyConfigs.size(); i++) {
        stream << keyConfigs[i].name << ",";
    }

    stream << "\n\n";

    u32 keyCount = (u32) inputGetKeyCount();
    for(u32 i = 0; i < keyConfigs.size(); i++) {
        KeyConfig& keyConfig = keyConfigs[i];

        stream << "[" << keyConfig.name << "]\n";
        for(u32 j = 0; j < keyCount; j++) {
            if(inputIsValidKey(j)) {
                stream << inputGetKeyName(j) << "=" << funcKeyNames[keyConfig.funcKeys[j]] << "\n";
            }
        }

        stream << "\n";
    }
}

void configLoadOptions(INIReader &reader) {
    for(int i = 0; i < numMenus; i++) {
        for(int j = 0; j < menuList[i].numOptions; j++) {
            if(menuList[i].options[j].numValues != 0) {
                int value = (int) reader.GetInteger("options", menuList[i].options[j].name, -1);
                if(value != -1) {
                    menuList[i].options[j].selection = value;
                    menuList[i].options[j].function(value);
                }
            }
        }
    }
}

void configSaveOptions(std::ofstream &stream) {
    stream << "[options]\n";

    for(int i = 0; i < numMenus; i++) {
        for(int j = 0; j < menuList[i].numOptions; j++) {
            if(menuList[i].options[j].numValues != 0) {
                stream << menuList[i].options[j].name << "=" << menuList[i].options[j].selection << "\n";
            }
        }
    }

    stream << "\n";
}

bool configLoad() {
    borderPath = systemDefaultBorderPath();
    romPath = systemDefaultRomPath();

    keyConfigs.clear();

    INIReader reader(systemIniPath());
    if(reader.ParseError() < 0) {
        configValidateControls();

        printMenuMessage("Error loading gameyob.ini.");
        systemPrintDebug("Failed to load gameyob.ini: %s\n", strerror(errno));
        return false;
    }

    configLoadGeneral(reader);
    configLoadControls(reader);
    configLoadOptions(reader);

    return true;
}

void configSave() {
    std::ofstream stream(systemIniPath());
    if(!stream.is_open()) {
        printMenuMessage("Error saving gameyob.ini.");
        systemPrintDebug("Failed to save gameyob.ini: %s\n", strerror(errno));
        return;
    }

    configSaveGeneral(stream);
    configSaveControls(stream);
    configSaveOptions(stream);

    stream.close();

    if(gameboy->cartridge != NULL) {
        cheatEngine->saveCheats(mgrGetRomName() + ".cht");
    }
}

std::string& configGetRomPath() {
    return romPath;
}

std::string& configGetBorderPath() {
    return borderPath;
}

void configSetBorderPath(std::string& path) {
    borderPath = path;
}

u32 configGetKeyConfigCount() {
    return (u32) keyConfigs.size();
}

KeyConfig* configGetKeyConfig(u32 num) {
    return &keyConfigs[num];
}

void configAddKeyConfig(const std::string& name) {
    keyConfigs.push_back(KeyConfig());
    keyConfigs.back().name = name;

    configValidateControls();
}

void configRemoveKeyConfig(u32 num) {
    keyConfigs.erase(keyConfigs.begin() + num);

    configValidateControls();
}

u32 configGetSelectedKeyConfig() {
    return selectedKeyConfig;
}

void configSetSelectedKeyConfig(u32 config) {
    selectedKeyConfig = config;

    configValidateControls();
}

const char* configGetFuncKeyName(u8 funcKey) {
    if(funcKey >= NUM_FUNC_KEYS) {
        return "";
    }

    return funcKeyNames[funcKey];
}