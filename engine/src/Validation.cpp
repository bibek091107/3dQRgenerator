#include "Validation.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace validation {

// ─────────────────────────────────────────────
//  Trim — App.cpp:24
// ─────────────────────────────────────────────

void Trim(std::string& s) {
    s.erase(s.begin(),
        std::find_if(s.begin(), s.end(), [](unsigned char c){ return !std::isspace(c); }));
    s.erase(
        std::find_if(s.rbegin(), s.rend(), [](unsigned char c){ return !std::isspace(c); }).base(),
        s.end());
}

std::string Trimmed(std::string s) {
    Trim(s);
    return s;
}

// ─────────────────────────────────────────────
//  "N/A" defaults — App.cpp:341-343 and 644-647
// ─────────────────────────────────────────────

void ApplyHolderDefaults(QRObject& obj) {
    if (obj.calibrationStatus.empty()) obj.calibrationStatus = "N/A";
    if (obj.accumulatedLife.empty())   obj.accumulatedLife   = "N/A";
    if (obj.serialBatchNumber.empty()) obj.serialBatchNumber = "N/A";
}

void ApplyToolDefaults(QRObject& obj) {
    if (obj.trayNo.empty())        obj.trayNo        = "N/A";
    if (obj.cncMachineId.empty())   obj.cncMachineId   = "N/A";
    if (obj.cncMachineName.empty()) obj.cncMachineName = "N/A";
    if (obj.operationType.empty())  obj.operationType  = "N/A";
}

// ─────────────────────────────────────────────
//  Numeric helper — mirrors the original try/catch around std::stod
// ─────────────────────────────────────────────

static bool ParseDoubleLoose(const std::string& s, double& out) {
    if (s.empty()) { out = 0.0; return true; }
    try {
        size_t pos = 0;
        double v = std::stod(s, &pos);
        (void)pos;
        out = v;
        return true;
    } catch (...) {
        return false;
    }
}

bool ExpectedRemainingLife(const std::string& toolLife,
                           const std::string& completedLife,
                           double& remainingOut,
                           bool& warning)
{
    double tl = 0.0, cl = 0.0;
    warning = false;
    remainingOut = 0.0;

    if (toolLife.empty() || completedLife.empty()) return false;
    if (!ParseDoubleLoose(toolLife, tl)) return false;
    if (!ParseDoubleLoose(completedLife, cl)) return false;
    if (tl < 0 || cl < 0) return false;

    remainingOut = tl - cl;
    if (remainingOut < 0) warning = true;
    return true;
}

// ─────────────────────────────────────────────
//  Normalize + validate
// ─────────────────────────────────────────────

Result CheckRequiredFields(const RawFields& raw, std::string& idOut, std::string& nameOut) {
    Result res;
    idOut   = Trimmed(raw.id);
    nameOut = Trimmed(raw.name);

    // ID and Name cannot be empty — "Empty Field" popup (App.cpp:345 / 546)
    if (idOut.empty() || nameOut.empty()) {
        res.ok      = false;
        res.code    = kEmptyField;
        res.message = "ID and Name cannot be empty.\n\nPlease enter valid information.";
    }
    return res;
}

Result NormalizeAndValidate(const std::string& type, const RawFields& raw, QRObject& out) {
    Result res;
    out = QRObject();
    out.type = type;

    out.id   = Trimmed(raw.id);
    out.name = Trimmed(raw.name);

    // ID and Name cannot be empty — "Empty Field" popup (App.cpp:345 / 546)
    if (out.id.empty() || out.name.empty()) {
        res.ok = false;
        res.code = kEmptyField;
        res.message = "ID and Name cannot be empty.\n\nPlease enter valid information.";
        return res;
    }

    if (type == "Holder") {
        out.calibrationStatus = Trimmed(raw.calibrationStatus);
        out.accumulatedLife   = Trimmed(raw.accumulatedLife);
        out.serialBatchNumber = Trimmed(raw.serialBatchNumber);
        ApplyHolderDefaults(out);
        return res;   // ok
    }

    // ── Tool ───────────────────────────────────────────────
    out.cncMachineId   = Trimmed(raw.cncMachineId);
    out.cncMachineName = Trimmed(raw.cncMachineName);
    out.operationType  = Trimmed(raw.operationType);
    out.trayNo         = Trimmed(raw.trayNo);

    std::string toolLifeStr = Trimmed(raw.toolLife);
    std::string compLifeStr = Trimmed(raw.completedLife);
    std::string visLifeStr  = Trimmed(raw.remainingVisual);
    std::string mlLifeStr   = Trimmed(raw.remainingML);
    std::string usageStr    = Trimmed(raw.totalUsageHours);

    double toolLife = 0.0, compLife = 0.0, visLife = 0.0, mlLife = 0.0, usageHours = 0.0;

    // Parse errors — message text copied from App.cpp:564-602
    if (!ParseDoubleLoose(toolLifeStr, toolLife)) {
        res.ok = false; res.code = kValidationError;
        res.message = "Tool Life (meters) must be a valid number.";
        return res;
    }
    if (!ParseDoubleLoose(compLifeStr, compLife)) {
        res.ok = false; res.code = kValidationError;
        res.message = "Completed Life (meters) must be a valid number.";
        return res;
    }
    if (!ParseDoubleLoose(visLifeStr, visLife)) {
        res.ok = false; res.code = kValidationError;
        res.message = "Remaining Life - Visual Inspection must be a valid number.";
        return res;
    }
    if (!ParseDoubleLoose(mlLifeStr, mlLife)) {
        res.ok = false; res.code = kValidationError;
        res.message = "Remaining Life - ML Model must be a valid number.";
        return res;
    }
    if (!ParseDoubleLoose(usageStr, usageHours)) {
        res.ok = false; res.code = kValidationError;
        res.message = "Total Hours of Usage must be a valid number.";
        return res;
    }

    // Range checks — message text copied from App.cpp:604-642
    if (toolLife < 0.0) {
        res.ok = false; res.code = kValidationError;
        res.message = "Tool Life (meters) cannot be negative.";
        return res;
    }
    if (compLife < 0.0) {
        res.ok = false; res.code = kValidationError;
        res.message = "Completed Life (meters) cannot be negative.";
        return res;
    }
    if (compLife > toolLife) {
        char errBuf[256];
        snprintf(errBuf, sizeof(errBuf),
            "Completed Life (%.2f m) cannot exceed Tool Life (%.2f m).\n\nPlease check your inputs.",
            compLife, toolLife);
        res.ok = false; res.code = kValidationError;
        res.message = errBuf;
        return res;
    }
    if (visLife < 0.0) {
        res.ok = false; res.code = kValidationError;
        res.message = "Remaining Life - Visual Inspection cannot be negative.";
        return res;
    }
    if (mlLife < 0.0) {
        res.ok = false; res.code = kValidationError;
        res.message = "Remaining Life - ML Model cannot be negative.";
        return res;
    }
    if (usageHours < 0.0) {
        res.ok = false; res.code = kValidationError;
        res.message = "Total Hours of Usage cannot be negative.";
        return res;
    }

    out.toolLifeMeters            = toolLife;
    out.completedLifeMeters       = compLife;
    out.remainingLifeVisualMeters = visLife;
    out.remainingLifeMLMeters     = mlLife;
    out.totalUsageHours           = usageHours;

    ApplyToolDefaults(out);
    return res;   // ok
}

} // namespace validation
