#include "filesystem.h"
#include "server.h"

#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>

// Task handle
TaskHandle_t serverTaskHandle;

// ====================================================== //
// ==================== GPU emulator ==================== //
// ====================================================== //

// Display
TFT_eSPI tft = TFT_eSPI();

void displayTest()
{
    // ─── Initialize Display ──────────────────────────────────────────────
    tft.init();
    tft.setRotation(1);

    tft.fillScreen(TFT_BLACK);

    // Set "cursor" at top left corner of display (0,0) and select font 2
    // (cursor will move to next line automatically during printing with
    // 'tft.println'
    //  or stay on the line is there is room for the text with tft.print)
    tft.setCursor(0, 0, 2);
    // Set the font colour to be white with a black background, set text size
    // multiplier to 1
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(1);
    // We can now plot text on screen using the "print" class
    tft.println("Hello World!");

    // Set the font colour to be yellow with no background, set to font 7
    tft.setTextColor(TFT_YELLOW);
    tft.setTextFont(2);
    tft.println(1234.56);

    // Set the font colour to be red with black background, set to font 4
    tft.setTextColor(TFT_RED, TFT_BLACK);
    tft.setTextFont(4);
    tft.println((long) 3735928559, HEX);  // Should print DEADBEEF

    // Set the font colour to be green with black background, set to font 2
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setTextFont(2);
    tft.println("Groop");

    // Test some print formatting functions
    float fnumber = 123.45;
    // Set the font colour to be blue with no background, set to font 2
    tft.setTextColor(TFT_BLUE);
    tft.setTextFont(2);
    tft.print("Float = ");
    tft.println(fnumber);             // Print floating point number
    tft.print("Binary = ");
    tft.println((int) fnumber, BIN);  // Print as integer value in binary
    tft.print("Hexadecimal = ");
    tft.println((int) fnumber, HEX);  // Print as integer number in Hexadecimal
}

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap)
{
    // Stop further decoding as image is running off bottom of screen
    if (y >= tft.height())
        return 0;

    // This function will clip the image block rendering automatically at the
    // TFT boundaries
    tft.pushImage(x, y, w, h, bitmap);

    // Return 1 to decode next block
    return 1;
}

void displayImageTest()
{
    // ─── Initialize Display ──────────────────────────────────────────────
    tft.init();
    tft.setRotation(1);

    tft.fillScreen(TFT_BLACK);
    tft.setSwapBytes(true);

    TJpgDec.setJpgScale(1);
    TJpgDec.setCallback(tft_output);

    // open image file
    uint16_t w = 0, h = 0;
    TJpgDec.getSdJpgSize(&w, &h, "/images.jpg");
    Serial.println("Image has size: " + String(w) + "x" + String(h));

    // decode and show image
    TJpgDec.drawSdJpg(0, 0, "/images.jpg");
}

void setup()
{
    Serial.begin(115200);

    delay(3000);  // Wait for me to open the serial monitor ;)

    // ─── Setup micro SD ───────────────────────────────────────────────────

    setupSD();

    // ─── Start Server Task ───────────────────────────────────────────────

    xTaskCreatePinnedToCore(
            serverTask, "Server Task", 10000, NULL, 1, &serverTaskHandle, 0);
}

void loop()
{
    // ─── Start Gpu Emulator ──────────────────────────────────────────────

    displayImageTest();

    while (1)
        yield();  // avoid watchdog resets
}