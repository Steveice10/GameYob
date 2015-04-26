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
