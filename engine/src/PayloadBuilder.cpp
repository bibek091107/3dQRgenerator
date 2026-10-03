#include "PayloadBuilder.h"

#include <cstdio>

// NOTE: The string layout below is copied verbatim from App::GenerateAllQRs()
// in the original 3DQRGenerator/src/App.cpp so that every QR code produced by
// this engine decodes to exactly the same payload as the desktop application.

std::string PayloadBuilder::FormatFixed2(double v) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f", v);
    return std::string(buf);
}

std::string PayloadBuilder::BuildPayload(const QRObject& obj) {
    if (obj.type == "Holder") {
        return "Type: " + obj.type + "\n"
             + "ID: " + obj.id + "\n"
             + "Name: " + obj.name + "\n"
             + "Calibration / Inspection Status: " + obj.calibrationStatus + "\n"
             + "Current Accumulated Life: " + obj.accumulatedLife + "\n"
             + "Serial / Batch Number: " + obj.serialBatchNumber;
    }

    // Tool — doubles rendered with "%.2f", exactly like the desktop app.
    return "Type: " + obj.type + "\n"
         + "Tool ID: " + obj.id + "\n"
         + "Tool Name: " + obj.name + "\n"
         + "Tool Life (meters): " + FormatFixed2(obj.toolLifeMeters) + "\n"
         + "Completed Life (meters): " + FormatFixed2(obj.completedLifeMeters) + "\n"
         + "Remaining Life (Visual Inspection): " + FormatFixed2(obj.remainingLifeVisualMeters) + " meters\n"
         + "Remaining Life (ML Model): " + FormatFixed2(obj.remainingLifeMLMeters) + " meters\n"
         + "Total Hours of Usage: " + FormatFixed2(obj.totalUsageHours) + " hours\n"
         + "CNC Machine ID: " + obj.cncMachineId + "\n"
         + "CNC Machine Name: " + obj.cncMachineName + "\n"
         + "Type of Operation: " + obj.operationType + "\n"
         + "Tray No: " + obj.trayNo;
}
