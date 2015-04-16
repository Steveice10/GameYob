#pragma once

#include <ctrcommon/types.hpp>

void initGbPrinter();
u8 sendGbPrinterByte(u8 dat);
void updateGbPrinter(); // Called each vblank
