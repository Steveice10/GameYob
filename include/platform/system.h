#pragma once

#include <string>

#include "types.h"

bool systemInit();
void systemExit();
void systemRun();

void systemCheckRunning();

int systemGetConsoleWidth();
int systemGetConsoleHeight();
void systemUpdateConsole();

bool systemGetIRState();
void systemSetIRState(bool state);

const std::string systemGetIP();
int systemSocketListen(u16 port);
FILE* systemSocketAccept(int listeningSocket, std::string* acceptedIp);
FILE* systemSocketConnect(const std::string ipAddress, u16 port);