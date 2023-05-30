#pragma once

// ====================================================== //
// ================= VGA Utility Library ================ //
// ====================================================== //

#include <DDCVCP.h>
#include <TFT_eSPI.h>
#include <PNGdec.h>

struct Color
{
    unsigned char r, g, b;
};

struct VGAMode
{
    int xres;
    int yres;

    // Horizontal Timings in us
    double hPixel;
    double hFrontPorch;
    double hSyncTime;
    double hBackPorch;

    // Vertical Timings in ms
    double vLine;
    double vFrontPorch;
    double vSyncTime;
    double vBackPorch;
};

struct Image
{
    int    xres{};
    int    yres{};
    Color *pixels{};

    Image(int xres = 640, int yres = 480)
        : xres(xres), yres(yres), pixels(new Color[xres * yres])
    {}

    ~Image()
    {
        delete[] pixels;
    }

    void loadPixels(int xres, int yres, Color *pixels)
    {
        delete[] this->pixels;
        this->xres   = xres;
        this->yres   = yres;
        this->pixels = pixels;
    }

    Color get(int x, int y) const
    {
        return pixels[y * xres + x];
    }

    void set(int x, int y, Color c)
    {
        pixels[y * xres + x] = c;
    }
};


extern Image         image;
extern TFT_eSPI      tft;
extern const VGAMode VGA480p;

extern const int      hsyncPin;
extern const int      vsyncPin;
extern const int      blankPin;
extern const int      dacClkPin;
extern const int      syncPin;
extern const int      ddcSdaPin;
extern const int      ddcSclPin;
extern const int      leBPin;
extern const int      leGPin;
extern const int      leRPin;
extern const int      colorPins[8];
extern const int      colorPinsMask;
extern int            xres, xcrt;
extern int            yres, ycrt;
extern hw_timer_t    *hPixelTimer;
extern hw_timer_t    *hSyncTimer;
extern hw_timer_t    *vSyncTimer;
extern hw_timer_t    *clkTimer;
extern const uint64_t pixelTicks;
extern uint64_t       crtTicks;
extern uint64_t       ticksElapsed;
extern int            dacClk;

void IRAM_ATTR writeByteToRegister(int latch, unsigned char byte);
void IRAM_ATTR writePixel();
void IRAM_ATTR hSyncInt();
void IRAM_ATTR vSyncInt();
void           tftInit();
void           drawPng(PNGDRAW *pDraw);
bool           decodePng(const char *filepath);

class VGASignal
{
private:
    DDCVCP ddc;

public:
    VGASignal()
    {
        // Start DDC communication
        // if (!ddc.begin(ddcSdaPin, ddcSclPin)) {
        //     Serial.println("Unable to find DDC/CI");
        // }

        // Write HIGH to SYNC pin for Generic DAC mode
        pinMode(syncPin, OUTPUT);
        digitalWrite(syncPin, HIGH);

        // Set output pins
        pinMode(hsyncPin, OUTPUT);
        pinMode(vsyncPin, OUTPUT);
        pinMode(blankPin, OUTPUT);
        pinMode(dacClk, OUTPUT);
        pinMode(leBPin, OUTPUT);
        pinMode(leGPin, OUTPUT);
        pinMode(leRPin, OUTPUT);
        for (int i = 0; i < 8; ++i)
            pinMode(colorPins[i], OUTPUT);
    }

    void setup()
    {
        // Reset signals
        digitalWrite(hsyncPin, LOW);
        digitalWrite(vsyncPin, LOW);
        digitalWrite(blankPin, HIGH);
        digitalWrite(dacClk, LOW);
        digitalWrite(leBPin, LOW);
        digitalWrite(leGPin, LOW);
        digitalWrite(leRPin, LOW);

        // Pixel clock interrupt
        if (!hPixelTimer) {
            hPixelTimer = timerBegin(1, 2, true);
            Serial.println("Pixel clock timer started");
        }

        // Horizontal sync interrupt
        if (!hSyncTimer) {
            hSyncTimer = timerBegin(3, 2, true);
            timerAttachInterrupt(hSyncTimer, &hSyncInt, false);
            timerAlarmWrite(
                    hSyncTimer, 40.0 * VGA480p.xres * VGA480p.hPixel, true);
            timerAlarmEnable(hSyncTimer);
            Serial.println("Horizontal sync timer started");
        }

        // Vertical sync interrupt
        if (!vSyncTimer) {
            vSyncTimer = timerBegin(2, 2, true);
            timerAttachInterrupt(vSyncTimer, &vSyncInt, false);
            timerAlarmWrite(
                    vSyncTimer, 40000.0 * VGA480p.yres * VGA480p.vLine, true);
            timerAlarmEnable(vSyncTimer);
            Serial.println("Vertical sync timer started");
        }

        // Reset timers
        timerRestart(hSyncTimer);
        timerRestart(vSyncTimer);
        timerRestart(hPixelTimer);
        ticksElapsed = crtTicks = 0;
    }

    void getMonitorResolution(int *width, int *height)
    {
        *width  = ddc.getVCP(0x22);
        *height = ddc.getVCP(0x32);
    }
};

extern VGASignal vga;