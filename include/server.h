#pragma once

// ====================================================== //
// ===================== Web Server ===================== //
// ====================================================== //

#include "vga.h"

// VGA class
extern VGASignal vga;

void serverTask(void *args);