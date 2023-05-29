#pragma once

// ====================================================== //
// ================= VGA Utility Library ================ //
// ====================================================== //

#include <DDCVCP.h>

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

    double pixelTime;
};

struct Image
{
    int    xres{};
    int    yres{};
    Color *pixels{};

    ~Image()
    {
        delete[] pixels;
    }

    void loadPixels(int xres, int yres, Color *pixels)
    {
        this->xres   = xres;
        this->yres   = yres;
        this->pixels = pixels;
    }

    Color get(int x, int y) const
    {
        return pixels[y * xres + x];
    }
};
extern Image image;


class VGASignal
{
private:
    DDCVCP               ddc;
    static const VGAMode VGA480p;

public:
    VGASignal()
    {
        // Start DDC communication
        while (!ddc.begin(DDC_SDA, DDC_SCL)) {
            Serial.println("Unable to find DDC/CI");
            delay(1000);
        }

        // Set output pins
        pinMode(hsyncPin, OUTPUT);
        pinMode(vsyncPin, OUTPUT);
        pinMode(blankPin, OUTPUT);
        pinMode(dacClk, OUTPUT);
        pinMode(syncPin, OUTPUT);
        pinMode(leB, OUTPUT);
        pinMode(leG, OUTPUT);
        pinMode(leR, OUTPUT);
        for (int i = 0; i < 8; ++i)
            pinMode(colorPins[i], OUTPUT);

        // Start timers and attach first interrupt
        hTimer = timerBegin(3, 1, true);
        vTimer = timerBegin(2, 1, true);

        timerAttachInterrupt(hTimer, &writePixel, true);
        timerAlarmWrite(hTimer, 80.0 * VGA480p.hPixel, true);
        timerAlarmEnable(hTimer);
    }

    void getMonitorResolution(int *width, int *height)
    {
        *width  = ddc.getVCP(0x22);
        *height = ddc.getVCP(0x32);
    }

    // ─── Interrupts
    // ──────────────────────────────────────────────────────

private:
    static const int hsyncPin = 1;
    static const int vsyncPin = 2;
    static const int blankPin = 42;
    static const int dacClk   = 41;
    static const int syncPin  = 38;

    static const int DDC_SDA = 40;
    static const int DDC_SCL = 39;

    static const int leB = 13;
    static const int leG = 14;
    static const int leR = 40;
    static const int colorPins[8];
    static const int colorPinsMask;

    static int xres, xcrt;
    static int yres, ycrt;

    static hw_timer_t *hTimer;
    static hw_timer_t *vTimer;

public:
    static void IRAM_ATTR writeByteToRegister(int latch, unsigned char byte)
    {
        unsigned int mask = 0;
        for (int i = 0; i < 8; ++i, byte >>= 1) {
            mask |= (1u << colorPins[i]) & (byte & 1u);
        }

        REG_WRITE(GPIO_OUT_W1TC_REG, colorPinsMask);
        digitalWrite(latch, HIGH);
        REG_WRITE(GPIO_OUT_W1TS_REG, mask & colorPinsMask);
        //! TODO: insert delay here
        digitalWrite(latch, LOW);
    }

    static void IRAM_ATTR writePixel()
    {
        Color pixel = image.get(xcrt, ycrt);
        writeByteToRegister(leR, pixel.r);
        writeByteToRegister(leG, pixel.g);
        writeByteToRegister(leB, pixel.b);

        // advance to next pixel
        if (++ycrt >= yres) {
            // line ended
            ++xcrt, ycrt = 0;
            timerAlarmDisable(hTimer);
            timerDetachInterrupt(hTimer);
            timerAttachInterrupt(hTimer, &hFrontPorchInt, true);
            timerAlarmWrite(hTimer, 80.0 * VGA480p.hFrontPorch, true);
            timerAlarmEnable(hTimer);
        }
    }

    static void IRAM_ATTR hFrontPorchInt()
    {
        timerAlarmDisable(hTimer);
        timerDetachInterrupt(hTimer);
        timerAttachInterrupt(hTimer, &hSyncInt, true);
        timerAlarmWrite(hTimer, 80.0 * VGA480p.hSyncTime, true);
        timerAlarmEnable(hTimer);
    }

    static void IRAM_ATTR hSyncInt()
    {
        timerAlarmDisable(hTimer);
        timerDetachInterrupt(hTimer);
        timerAttachInterrupt(hTimer, &hBackPorchInt, true);
        timerAlarmWrite(hTimer, 80.0 * VGA480p.hBackPorch, true);
        timerAlarmEnable(hTimer);
    }

    static void IRAM_ATTR hBackPorchInt()
    {
        if (xcrt >= xres) {
            // frame ended
            xcrt = 0, ycrt = 0;
            timerAlarmDisable(hTimer);
            timerDetachInterrupt(hTimer);
            timerAttachInterrupt(vTimer, &vFrontPorchInt, true);
            timerAlarmWrite(vTimer, 80.0 * VGA480p.vFrontPorch, true);
            timerAlarmEnable(vTimer);
        }
        else {
            // start next line
            timerAlarmDisable(hTimer);
            timerDetachInterrupt(hTimer);
            timerAttachInterrupt(hTimer, &writePixel, true);
            timerAlarmWrite(hTimer, 80.0 * VGA480p.hPixel, true);
            timerAlarmEnable(hTimer);
        }
    }

    static void IRAM_ATTR vFrontPorchInt()
    {
        timerAlarmDisable(vTimer);
        timerDetachInterrupt(vTimer);
        timerAttachInterrupt(vTimer, &vSyncInt, true);
        timerAlarmWrite(vTimer, 80.0 * VGA480p.vSyncTime, true);
        timerAlarmEnable(vTimer);
    }

    static void IRAM_ATTR vSyncInt()
    {
        timerAlarmDisable(vTimer);
        timerDetachInterrupt(vTimer);
        timerAttachInterrupt(vTimer, &vBackPorchInt, true);
        timerAlarmWrite(vTimer, 80.0 * VGA480p.vBackPorch, true);
        timerAlarmEnable(vTimer);
    }

    static void IRAM_ATTR vBackPorchInt()
    {
        timerAlarmDisable(vTimer);
        timerDetachInterrupt(vTimer);
        timerAttachInterrupt(hTimer, &writePixel, true);
        timerAlarmWrite(hTimer, 80.0 * VGA480p.hPixel, true);
        timerAlarmEnable(hTimer);
    }
};