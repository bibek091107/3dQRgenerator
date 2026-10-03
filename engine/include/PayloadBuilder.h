#pragma once

// QR payload generation.
//
// Extracted verbatim from App::GenerateAllQRs() in the original
// 3DQRGenerator/src/App.cpp. The payload format is byte-for-byte identical to
// the desktop application — do not reformat these strings.

#include "QRObject.h"
#include <string>

class PayloadBuilder {
public:
    // Builds the plain-text payload encoded into the QR code.
    // Holder and Tool layouts are exactly the ones used by the desktop app.
    static std::string BuildPayload(const QRObject& obj);

    // Formats a double the same way the desktop app did (snprintf "%.2f").
    static std::string FormatFixed2(double v);
};
