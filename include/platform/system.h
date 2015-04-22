#pragma once

#include <string>

bool systemInit();
void systemExit();
void systemRun();

void systemCheckRunning();

int systemGetConsoleWidth();
int systemGetConsoleHeight();
void systemUpdateConsole();
