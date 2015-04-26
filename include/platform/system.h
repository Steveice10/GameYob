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

u32 systemGetIRState();
void systemSetIRState(u32 state);
