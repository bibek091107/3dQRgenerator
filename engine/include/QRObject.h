#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include "Mesh.h"

struct QRObject {
    std::string type;   // "Holder" or "Tool"
    std::string id;
    std::string name;
    // Holder fields (preserved for Holder)
    std::string calibrationStatus;   // Calibration / Inspection Status
    std::string accumulatedLife;     // Current Accumulated Life (e.g. 120.5 minutes, 450 parts cut)
    std::string serialBatchNumber;   // Serial Number / Batch Number (traceability code)

    // Tool fields
    double toolLifeMeters            = 0.0; // Total expected tool life in meters
    double completedLifeMeters       = 0.0; // Consumed tool life in meters (<= toolLifeMeters)
    double remainingLifeVisualMeters = 0.0; // Remaining life according to Visual Inspection (meters)
    double remainingLifeMLMeters     = 0.0; // Remaining life according to ML Model (meters)
    double totalUsageHours           = 0.0; // Cumulative usage hours
    std::string cncMachineId;               // CNC Machine ID (e.g. CNC001)
    std::string cncMachineName;             // CNC Machine Name (e.g. Haas VF-2)
    std::string operationType;              // Type of Operation (predefined or custom)
    std::string trayNo;                     // Tray Number (e.g. TR-01)

    float sizeMM  = 9.0f;
    float depthMM = 0.8f;

    std::string payload; // Plain-text QR payload

    std::vector<std::vector<bool>> qrMatrix; // Raw QR bit-matrix (shared for PNG + STL)

    Mesh mesh;           // 3D geometry

    std::filesystem::path stlPath; // Canonical STL path on disk
    std::filesystem::path pngPath; // Canonical PNG path on disk

    bool stlGenerated = false;
    bool pngGenerated = false;
};
