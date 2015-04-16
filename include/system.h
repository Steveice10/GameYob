#pragma once

#include <stdarg.h>
#include <stdio.h>

#define FAT_CACHE_SIZE 16

extern char borderPath[256];

bool systemInit();
void systemCleanup();

int systemGetConsoleWidth();
int systemGetConsoleHeight();
void systemUpdateConsole();

void systemCheckPolls();

void systemDisableSleepMode();
void systemEnableSleepMode();

void systemFlushFatCache();
