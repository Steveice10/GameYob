#pragma once

#include <string>

bool systemInit();
void systemExit();
void systemRun();

int systemGetConsoleWidth();
int systemGetConsoleHeight();
void systemUpdateConsole();

void systemCheckRunning();

void systemDisableSleepMode();
void systemEnableSleepMode();

bool systemExists(std::string path);
bool systemIsDirectory(std::string path);
void systemDelete(std::string path);
