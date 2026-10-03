#pragma once
// Minimal, dependency-free PNG writer.
// Writes an 8-bit grayscale (1 channel) or RGB (3 channels) PNG.
// Only uses zlib (available on every platform via the system or bundled).

#include <string>
#include <vector>
#include <cstdint>

class PNGWriter {
public:
    // Write a black-and-white PNG from a boolean QR matrix.
    // true  = QR module (BLACK pixel)
    // false = background  (WHITE pixel)
    // scale = how many pixels per QR module (e.g. 10 = each cell is 10x10 px)
    // quietZone = extra white-cell border (in modules)
    static bool WriteQRPNG(
        const std::string& filepath,
        const std::vector<std::vector<bool>>& qrMatrix,
        int scale     = 10,
        int quietZone = 4
    );
};
