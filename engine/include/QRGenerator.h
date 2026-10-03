#pragma once

#include <string>
#include <vector>

class QRGenerator {
public:
    // Generates a boolean 2D matrix representing the QR code (true = dark, false = light)
    static std::vector<std::vector<bool>> GenerateMatrix(const std::string& payload);
};
