#include "vga.h"
#include "filesystem.h"

const int      hsyncPin      = 1;
const int      vsyncPin      = 2;
const int      blankPin      = 42;
const int      dacClkPin     = 41;
const int      syncPin       = 38;
const int      ddcSdaPin     = 40;
const int      ddcSclPin     = 39;
const int      leBPin        = 13;
const int      leGPin        = 14;
const int      leRPin        = 40;
const int      colorPins[8]  = { 16, 17, 18, 8, 9, 10, 11, 12 };
const int      colorPinsMask = 0b1110001111100000000;
int            xres, xcrt;
int            yres, ycrt;
int            dacClk;
const uint64_t pixelTicks = 40.0 * VGA480p.hPixel;
uint64_t       crtTicks;
uint64_t       ticksElapsed;
hw_timer_t    *hPixelTimer{};
hw_timer_t    *hSyncTimer{};
hw_timer_t    *vSyncTimer{};

const VGAMode VGA480p = { 640, 480, 25.422045680238 / 640.0, 0.63555114200596,
    0.63555114200596, 1.9066534260179, 15.253227408143 / 480.0,
    0.31777557100298, 0.31777557100298, 1.0486593843098 };
Image         image;
VGASignal     vga;
TFT_eSPI      tft = TFT_eSPI();
PNG           pngdec;
File          pngFile;


void IRAM_ATTR writeByteToRegister(int latch, unsigned char byte)
{
    unsigned int mask = 0;
    for (int i = 0; i < 8; ++i, byte >>= 1) {
        mask |= (1u << colorPins[i]) & (byte & 1u);
    }

    REG_WRITE(GPIO_OUT_W1TC_REG, colorPinsMask);
    REG_WRITE(GPIO_OUT_W1TS_REG, mask & colorPinsMask);

    // busy wait
    // for (int i = 0; i < 3; ++i)
    //     ;
    REG_WRITE(GPIO_OUT_W1TS_REG, 1u << latch);

    // busy wait
    // for (int i = 0; i < 3; ++i)
    //     ;
    REG_WRITE(GPIO_OUT_W1TC_REG, 1u << latch);
}

void IRAM_ATTR writePixel()
{
    Color pixel = image.get(xcrt, ycrt);
    writeByteToRegister(leRPin, pixel.r);
    writeByteToRegister(leGPin, pixel.g);
    writeByteToRegister(leBPin, pixel.b);

    // advance to next pixel
    if ((ycrt += 4) >= yres) {
        // line ended
        xcrt += 4, ycrt = 0;
        if (xcrt >= xres) {
            // frame ended
            xcrt = 0;
        }
    }

    // write to DAC
    REG_WRITE(GPIO_OUT_W1TS_REG, 1u << dacClk);

    // busy wait
    // for (int i = 0; i < 3; ++i)
    //     ;
    REG_WRITE(GPIO_OUT_W1TC_REG, 1u << dacClk);
}

void IRAM_ATTR hSyncInt()
{
    uint64_t       ticksElapsed{};
    const uint64_t ticksFrontPorch = 40.0 * VGA480p.hFrontPorch;
    const uint64_t ticksSync       = ticksFrontPorch + 40.0 * VGA480p.hSyncTime;
    const uint64_t ticksBackPorch  = ticksSync + 40.0 * VGA480p.hBackPorch;

    // write blank
    REG_WRITE(GPIO_OUT_W1TC_REG, 1u << blankPin);

    // front porch
    do {
        ticksElapsed = timerRead(hSyncTimer);
    } while (ticksElapsed < ticksFrontPorch);

    // sync
    digitalWrite(hsyncPin, LOW);
    do {
        ticksElapsed = timerRead(hSyncTimer);
    } while (ticksElapsed < ticksSync);

    // back porch
    REG_WRITE(GPIO_OUT_W1TS_REG, 1u << hsyncPin);
    do {
        ticksElapsed = timerRead(hSyncTimer);
    } while (ticksElapsed < ticksBackPorch);

    // unset blank
    REG_WRITE(GPIO_OUT_W1TS_REG, 1u << blankPin);
}

void IRAM_ATTR vSyncInt()
{
    uint64_t       ticksElapsed{};
    const uint64_t ticksFrontPorch = 40000.0 * VGA480p.vFrontPorch;
    const uint64_t ticksSync = ticksFrontPorch + 40000.0 * VGA480p.vSyncTime;
    const uint64_t ticksBackPorch = ticksSync + 40000.0 * VGA480p.vBackPorch;

    // write blank
    REG_WRITE(GPIO_OUT_W1TC_REG, 1u << blankPin);

    // front porch
    do {
        ticksElapsed = timerRead(vSyncTimer);
    } while (ticksElapsed < ticksFrontPorch);

    // sync
    REG_WRITE(GPIO_OUT_W1TC_REG, 1u << vsyncPin);
    do {
        ticksElapsed = timerRead(vSyncTimer);
    } while (ticksElapsed < ticksSync);

    // back porch
    REG_WRITE(GPIO_OUT_W1TS_REG, 1u << vsyncPin);
    do {
        ticksElapsed = timerRead(vSyncTimer);
    } while (ticksElapsed < ticksBackPorch);

    // unset blank
    REG_WRITE(GPIO_OUT_W1TS_REG, 1u << blankPin);

    // reset pixel
    xcrt = ycrt = 0;

    // reset timer
    timerWrite(hSyncTimer, 0);
    ticksElapsed = crtTicks = 0;
}

void tftInit()
{
    // ─── Initialize Display ──────────────────────────────────────────────
    tft.init();
    tft.setRotation(1);

    tft.fillScreen(TFT_BLACK);
    tft.setSwapBytes(true);
}

void *pngOpen(const char *filepath, int32_t *size)
{
    Serial.printf("Opening file %s\n", filepath);
    pngFile = SD.open(filepath);
    if (!pngFile) {
        Serial.println("Failed to open file for reading");
        return nullptr;
    }
    *size = pngFile.size();
    return (void *) &pngFile;
}

void pngClose(void *pHandle)
{
    if (pngFile) {
        pngFile.close();
    }
}

int32_t pngRead(PNGFILE *pHandle, uint8_t *pBuf, int32_t bufSize)
{
    if (!pngFile) {
        return 0;
    }
    return pngFile.read(pBuf, bufSize);
}

int32_t pngSeek(PNGFILE *pHandle, int32_t offset)
{
    if (!pngFile) {
        return 0;
    }
    return pngFile.seek(offset);
}

void drawPng(PNGDRAW *pDraw)
{
    // draw on tft every 4th line
    if (pDraw->y % 4 == 0) {
        uint16_t usPixels[640];
        pngdec.getLineAsRGB565(
                pDraw, usPixels, PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);

        // resize to 160
        for (int i = 0; i < 160; ++i) {
            usPixels[i] = usPixels[i * 4];
        }

        tft.pushImage(0, pDraw->y / 4, 160, 1, usPixels);
    }

    // update image for vga
    for (int i = 0; i < 640; i++) {
        image.set(i, pDraw->y,
                Color{ (unsigned char) ((pDraw->pPixels[i] >> 16) & 0xff),
                        (unsigned char) ((pDraw->pPixels[i] >> 8) & 0xff),
                        (unsigned char) (pDraw->pPixels[i] & 0xff) });
    }
}

bool decodePng(const char *filepath)
{
    if (pngdec.open(filepath, pngOpen, pngClose, pngRead, pngSeek, drawPng)
            == PNG_SUCCESS) {
        Serial.printf("Image specs: (%d x %d), %d bpp, pixel type: %d\n",
                pngdec.getWidth(), pngdec.getHeight(), pngdec.getBpp(),
                pngdec.getPixelType());

        if (pngdec.decode(NULL, 0) != PNG_SUCCESS) {
            Serial.println("Failed to decode PNG");
            return false;
        }

        pngdec.close();
        return true;
    }
    else {
        Serial.println("Failed to open PNG");
        return false;
    }
}