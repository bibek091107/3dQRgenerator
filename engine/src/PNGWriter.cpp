// PNGWriter.cpp
// Minimal, self-contained PNG writer.
// Produces a valid PNG using raw-deflate zlib compression.
// No external image library is required — only the system zlib (available on macOS/Linux by default).

#include "PNGWriter.h"

#include <fstream>
#include <cstring>
#include <stdexcept>
#include <zlib.h>   // system zlib — available on macOS without any extra setup

// ─────────────────────────────────────────────
//  Internal helpers
// ─────────────────────────────────────────────

static void write_u32_be(std::vector<uint8_t>& buf, uint32_t v) {
    buf.push_back((v >> 24) & 0xFF);
    buf.push_back((v >> 16) & 0xFF);
    buf.push_back((v >>  8) & 0xFF);
    buf.push_back((v >>  0) & 0xFF);
}

static void write_bytes(std::vector<uint8_t>& buf, const uint8_t* data, size_t len) {
    buf.insert(buf.end(), data, data + len);
}

// Write a PNG chunk: length (4 bytes BE) + type (4 bytes) + data + CRC (4 bytes)
static void write_chunk(std::vector<uint8_t>& out,
                        const char type[4],
                        const std::vector<uint8_t>& data)
{
    write_u32_be(out, (uint32_t)data.size());
    const uint8_t* t = reinterpret_cast<const uint8_t*>(type);
    out.push_back(t[0]); out.push_back(t[1]);
    out.push_back(t[2]); out.push_back(t[3]);
    write_bytes(out, data.data(), data.size());
    // CRC over type + data
    uint32_t crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, t, 4);
    if (!data.empty())
        crc = crc32(crc, data.data(), (uInt)data.size());
    write_u32_be(out, crc);
}

// ─────────────────────────────────────────────
//  PNGWriter::WriteQRPNG
// ─────────────────────────────────────────────

bool PNGWriter::WriteQRPNG(
    const std::string& filepath,
    const std::vector<std::vector<bool>>& qrMatrix,
    int scale,
    int quietZone)
{
    if (qrMatrix.empty()) return false;

    int qrSize  = (int)qrMatrix.size();
    int imgSize = (qrSize + 2 * quietZone) * scale; // total image width/height in pixels

    // Build the raw scanlines (RGB, 3 bytes per pixel)
    // Each scanline is prefixed by a filter byte (0 = None)
    std::vector<uint8_t> raw;
    raw.reserve((size_t)(imgSize * (1 + imgSize * 3)));

    for (int py = 0; py < imgSize; ++py) {
        raw.push_back(0); // filter byte
        // Which QR row does this pixel row correspond to?
        int qy = py / scale - quietZone;
        for (int px = 0; px < imgSize; ++px) {
            int qx = px / scale - quietZone;
            bool isModule = false;
            if (qx >= 0 && qx < qrSize && qy >= 0 && qy < qrSize)
                isModule = qrMatrix[qy][qx];
            uint8_t v = isModule ? 0x00 : 0xFF; // BLACK module / WHITE background
            raw.push_back(v); // R
            raw.push_back(v); // G
            raw.push_back(v); // B
        }
    }

    // zlib-compress the raw scanlines
    uLongf compBound = compressBound((uLong)raw.size());
    std::vector<uint8_t> compressed(compBound);
    int zret = compress2(compressed.data(), &compBound,
                         raw.data(), (uLong)raw.size(),
                         Z_BEST_COMPRESSION);
    if (zret != Z_OK) return false;
    compressed.resize(compBound);

    // Assemble PNG file
    std::vector<uint8_t> png;
    png.reserve(compressed.size() + 200);

    // PNG signature
    const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    write_bytes(png, sig, 8);

    // IHDR
    {
        std::vector<uint8_t> ihdr;
        write_u32_be(ihdr, (uint32_t)imgSize); // width
        write_u32_be(ihdr, (uint32_t)imgSize); // height
        ihdr.push_back(8);  // bit depth
        ihdr.push_back(2);  // color type: RGB
        ihdr.push_back(0);  // compression
        ihdr.push_back(0);  // filter
        ihdr.push_back(0);  // interlace: none
        write_chunk(png, "IHDR", ihdr);
    }

    // IDAT (image data)
    {
        write_chunk(png, "IDAT", compressed);
    }

    // IEND
    {
        write_chunk(png, "IEND", {});
    }

    // Write to file
    std::ofstream f(filepath, std::ios::binary);
    if (!f.is_open()) return false;
    f.write(reinterpret_cast<const char*>(png.data()), (std::streamsize)png.size());
    return f.good();
}
