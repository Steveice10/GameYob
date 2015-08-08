#pragma once

#include "types.h"

extern const char* iniPath;
extern const char* defaultGbBiosPath;
extern const char* defaultGbcBiosPath;
extern const char* defaultBorderPath;
extern const char* defaultRomPath;

bool systemInit(int argc, char* argv[]);
void systemExit();
void systemRun();

void systemCheckRunning();

void systemPrintDebug(const char* fmt, ...);

bool systemGetIRState();
void systemSetIRState(bool state);

const std::string systemGetIP();
int systemSocketListen(u16 port);
FILE* systemSocketAccept(int listeningSocket, std::string* acceptedIp);
FILE* systemSocketConnect(const std::string ipAddress, u16 port);