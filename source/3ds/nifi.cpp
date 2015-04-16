#include "nifi.h"

volatile int linkReceivedData;
volatile int linkSendData;
volatile bool transferWaiting;
volatile bool transferReady;
volatile bool receivedPacket;
volatile int nifiSendid;
bool nifiEnabled;

void enableNifi() { }

void disableNifi() { }

void nifiSendPacket(u8 command, u8* data, int dataLen) { }

void nifiHostMenu(int value) { }

void nifiClientMenu(int value) { }

bool nifiIsHost() { return false; }

bool nifiIsClient() { return false; }

bool nifiIsLinked() { return false; }

void nifiCheckInput() { }

void nifiUpdateInput() { }
