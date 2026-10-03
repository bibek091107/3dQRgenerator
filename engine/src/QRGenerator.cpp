#include "QRGenerator.h"
#include "qrcodegen.hpp"

using namespace qrcodegen;

std::vector<std::vector<bool>> QRGenerator::GenerateMatrix(const std::string& payload) {
    // We use Error Correction Level Medium as requested (suitable for physical tags)
    QrCode qr = QrCode::encodeText(payload.c_str(), QrCode::Ecc::MEDIUM);

    int size = qr.getSize();
    std::vector<std::vector<bool>> matrix(size, std::vector<bool>(size, false));

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            matrix[y][x] = qr.getModule(x, y);
        }
    }

    return matrix;
}
