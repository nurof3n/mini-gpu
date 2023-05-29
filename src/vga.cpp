#include "vga.h"

const int     VGASignal::colorPins[8]  = { 16, 17, 18, 8, 9, 10, 11, 12 };
const int     VGASignal::colorPinsMask = 0b1110001111100000000;
const VGAMode VGASignal::VGA480p
        = { 640, 480, 25.422045680238 / 640.0, 0.63555114200596,
              0.63555114200596, 1.9066534260179, 15.253227408143 / 480.0,
              0.31777557100298, 0.31777557100298, 1.0486593843098 };
Image image;