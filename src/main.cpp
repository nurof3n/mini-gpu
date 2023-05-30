#include "filesystem.h"
#include "server.h"
#include "vga.h"


// Server task handle
TaskHandle_t serverTaskHandle;

void setup()
{
    Serial.begin(115200);

    delay(3000);  // Wait for me to open the serial monitor ;)

    // ─── Setup micro SD ───────────────────────────────────────────────────

    setupSD();

    // ─── Start Server Task ───────────────────────────────────────────────

    xTaskCreatePinnedToCore(
            serverTask, "Server Task", 10000, NULL, 1, &serverTaskHandle, 0);

    // ─── Initialize Tft Display ──────────────────────────────────────────

    tftInit();

    // ─── Load Png ────────────────────────────────────────────────────────

    if (!decodePng("/pm.png")) {
        Serial.println("Failed to load default image /pm.png");
    }
    else {
        Serial.println("Image loaded");

        // Start VGA emulator
        vga.setup();
    }
}

void loop()
{
    // 4 times as less pixels
    crtTicks = timerRead(hPixelTimer);
    if (crtTicks < ticksElapsed)
        crtTicks += APB_CLK_FREQ / 2;

    if (crtTicks - ticksElapsed >= pixelTicks * 4) {
        writePixel();
        ticksElapsed = crtTicks;
    }
}