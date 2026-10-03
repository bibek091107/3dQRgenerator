#pragma once

// Input normalization + validation.
//
// Extracted verbatim from the save handlers in the original
// 3DQRGenerator/src/App.cpp:
//   * Trim()                    (App.cpp:24)
//   * empty-ID/Name check       (App.cpp:345 / 546)
//   * "N/A" substitution        (App.cpp:341-343 / 644-647)
//   * Tool numeric validation   (App.cpp:557-642)
//
// The desktop app kept some of this inline in its ImGui handlers; here it is a
// reusable, UI-free function so the web backend and the desktop app behave
// identically.

#include "QRObject.h"
#include <string>
#include <vector>

namespace validation {

// Error codes surfaced to the UI. These map 1:1 to the desktop app's popups.
inline constexpr const char* kOk              = "ok";
inline constexpr const char* kEmptyField      = "empty_field";      // "Empty Field" popup
inline constexpr const char* kDuplicateId     = "duplicate_id";     // "Duplicate ID" popup
inline constexpr const char* kValidationError = "validation_error"; // "Validation Error" popup

struct Result {
    bool        ok      = true;
    std::string code    = kOk;   // one of the k* codes above
    std::string message;         // exact text shown in the desktop popup
};

// Desktop app's Trim(): strips leading/trailing whitespace, in place.
void Trim(std::string& s);
std::string Trimmed(std::string s);

// Holder "N/A" defaults — App.cpp:341-343
void ApplyHolderDefaults(QRObject& obj);

// Tool "N/A" defaults — App.cpp:644-647
void ApplyToolDefaults(QRObject& obj);

// The raw, per-type string fields as typed by the user (i.e. what the
// desktop app held in its m_Input* char buffers).
struct RawFields {
    std::string id;
    std::string name;

    // Holder
    std::string calibrationStatus;
    std::string accumulatedLife;
    std::string serialBatchNumber;

    // Tool
    std::string toolLife;
    std::string completedLife;
    std::string remainingVisual;
    std::string remainingML;
    std::string totalUsageHours;
    std::string cncMachineId;
    std::string cncMachineName;
    std::string operationType;
    std::string trayNo;
};

// Trims ID/Name and checks that neither is empty — App.cpp:345 / 546.
// On success `idOut`/`nameOut` hold the trimmed values.
//
// This is split out of NormalizeAndValidate because the desktop app checks for
// a duplicate ID *between* the empty-field check and the Tool numeric
// validation (App.cpp:540-574), so callers that own a duplicate-ID check need
// the empty-field stage on its own to reproduce that exact popup ordering.
Result CheckRequiredFields(const RawFields& raw,
                           std::string& idOut,
                           std::string& nameOut);

// Trims everything, checks ID/Name non-empty, applies the "N/A" defaults, and
// for Tools runs the numeric validation. On success `out` is fully populated.
// Numeric parsing mirrors the original: std::stod inside try/catch, so inputs
// like "12abc" parse as 12 exactly as before.
Result NormalizeAndValidate(const std::string& type,
                            const RawFields& raw,
                            QRObject& out);

// The "Expected Remaining Life" helper shown live in the Tool form.
// Returns false when either value is missing/invalid; sets `warning` when
// Completed Life exceeds Tool Life.
bool ExpectedRemainingLife(const std::string& toolLife,
                           const std::string& completedLife,
                           double& remainingOut,
                           bool& warning);

} // namespace validation
