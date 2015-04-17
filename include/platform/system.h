#pragma once

#include <stdarg.h>
#include <stdio.h>

extern char borderPath[256];

bool systemInit();
void systemExit();
void systemRun();

int systemGetConsoleWidth();
int systemGetConsoleHeight();
void systemUpdateConsole();

void systemCheckRunning();

void systemDisableSleepMode();
void systemEnableSleepMode();
