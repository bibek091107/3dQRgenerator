// qr_engine — CLI/service boundary around the 3D QR generation engine.
//
// Reads a single JSON request on stdin (or from a file via --in) and writes a
// single JSON response on stdout. This lets the FastAPI backend reuse the
// original C++ QR / 3D / STL algorithms without duplicating any of them.
//
// Commands:
//   ping      – health check
//   validate  – trim + defaults + numeric validation + ID-uniqueness check
//   generate  – payload + QR matrix + 3D mesh + canonical paths (batch)
//   export    – write one STL or PNG, with the desktop app's verification
//   mesh      – base64 9-float-per-vertex mesh buffer for the web 3D preview
//   exists    – filesystem ID-uniqueness probe
//   paths     – canonical output paths for a type/id

#include "Json.h"
#include "QRObject.h"
#include "EngineService.h"
#include "PayloadBuilder.h"
#include "OutputPaths.h"
#include "Validation.h"
#include "MeshBuilder.h"
#include "QRGenerator.h"
#include "STLWriter.h"
#include "PNGWriter.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>

using qrjson::Value;

static const char* kVersion = "1.0.0";

// ─────────────────────────────────────────────
//  QRObject <-> JSON
// ─────────────────────────────────────────────

static Value ObjectToJson(const QRObject& o) {
    Value v = Value::MakeObject();
    v.Set("type",                     o.type);
    v.Set("id",                       o.id);
    v.Set("name",                     o.name);
    v.Set("calibrationStatus",        o.calibrationStatus);
    v.Set("accumulatedLife",          o.accumulatedLife);
    v.Set("serialBatchNumber",        o.serialBatchNumber);
    v.Set("toolLifeMeters",           o.toolLifeMeters);
    v.Set("completedLifeMeters",      o.completedLifeMeters);
    v.Set("remainingLifeVisualMeters",o.remainingLifeVisualMeters);
    v.Set("remainingLifeMLMeters",    o.remainingLifeMLMeters);
    v.Set("totalUsageHours",          o.totalUsageHours);
    v.Set("cncMachineId",             o.cncMachineId);
    v.Set("cncMachineName",           o.cncMachineName);
    v.Set("operationType",            o.operationType);
    v.Set("trayNo",                   o.trayNo);
    v.Set("sizeMM",                   (double)o.sizeMM);
    v.Set("depthMM",                  (double)o.depthMM);
    v.Set("payload",                  o.payload);
    v.Set("stlRelativePath",          outputpaths::STLRelativePath(o.type, o.id));
    v.Set("pngRelativePath",          outputpaths::PNGRelativePath(o.type, o.id));
    return v;
}

static QRObject ObjectFromJson(const Value* v) {
    QRObject o;
    if (!v || !v->IsObject()) return o;
    o.type                     = v->GetString("type");
    o.id                       = v->GetString("id");
    o.name                     = v->GetString("name");
    o.calibrationStatus        = v->GetString("calibrationStatus");
    o.accumulatedLife          = v->GetString("accumulatedLife");
    o.serialBatchNumber        = v->GetString("serialBatchNumber");
    o.toolLifeMeters           = v->GetNumber("toolLifeMeters");
    o.completedLifeMeters      = v->GetNumber("completedLifeMeters");
    o.remainingLifeVisualMeters= v->GetNumber("remainingLifeVisualMeters");
    o.remainingLifeMLMeters    = v->GetNumber("remainingLifeMLMeters");
    o.totalUsageHours          = v->GetNumber("totalUsageHours");
    o.cncMachineId             = v->GetString("cncMachineId");
    o.cncMachineName           = v->GetString("cncMachineName");
    o.operationType            = v->GetString("operationType");
    o.trayNo                   = v->GetString("trayNo");
    o.sizeMM                   = (float)v->GetNumber("sizeMM", 9.0);
    o.depthMM                  = (float)v->GetNumber("depthMM", 0.8);
    return o;
}

static Value ErrorResponse(const std::string& code, const std::string& message) {
    Value r = Value::MakeObject();
    r.Set("ok", false);
    r.Set("code", code);
    r.Set("message", message);
    return r;
}

static Value OkResponse() {
    Value r = Value::MakeObject();
    r.Set("ok", true);
    r.Set("code", validation::kOk);
    r.Set("message", "");
    return r;
}

// ─────────────────────────────────────────────
//  Commands
// ─────────────────────────────────────────────

static Value CmdPing(const Value&) {
    Value r = OkResponse();
    r.Set("engine", "qr_engine");
    r.Set("version", kVersion);
    r.Set("qrImplementation", "nayuki/QR-Code-generator (Ecc::MEDIUM)");
    r.Set("pngDefaults", "scale=10, quietZone=4");
    r.Set("meshDefaults", "quietZone=2, baseThickness=0.5");
    return r;
}

static Value CmdValidate(const Value& req) {
    std::string type = req.GetString("type");
    std::string outputRoot = req.GetString("outputRoot");

    if (!outputpaths::IsValidType(type)) {
        return ErrorResponse(validation::kValidationError,
                             "Type must be either \"Holder\" or \"Tool\".");
    }

    const Value* f = req.Find("fields");
    if (!f || !f->IsObject()) {
        return ErrorResponse(validation::kValidationError, "Missing 'fields' object.");
    }

    validation::RawFields raw;
    raw.id                 = f->GetString("id");
    raw.name               = f->GetString("name");
    raw.calibrationStatus  = f->GetString("calibrationStatus");
    raw.accumulatedLife    = f->GetString("accumulatedLife");
    raw.serialBatchNumber  = f->GetString("serialBatchNumber");
    raw.toolLife           = f->GetString("toolLife");
    raw.completedLife      = f->GetString("completedLife");
    raw.remainingVisual    = f->GetString("remainingVisual");
    raw.remainingML        = f->GetString("remainingML");
    raw.totalUsageHours    = f->GetString("totalUsageHours");
    raw.cncMachineId       = f->GetString("cncMachineId");
    raw.cncMachineName     = f->GetString("cncMachineName");
    raw.operationType      = f->GetString("operationType");
    raw.trayNo             = f->GetString("trayNo");

    // ── Validation order mirrors the desktop save handlers exactly ──
    // App.cpp:540-574 checks, in this sequence:
    //   1. empty ID/Name   -> "Empty Field" popup
    //   2. duplicate ID    -> "Duplicate ID" popup
    //   3. numeric values  -> "Validation Error" popup
    // So a blank numeric field on an already-used ID must still report the
    // duplicate ID, not the numeric error.

    // 1. empty ID/Name
    std::string idChk, nameChk;
    validation::Result reqRes = validation::CheckRequiredFields(raw, idChk, nameChk);
    if (!reqRes.ok) return ErrorResponse(reqRes.code, reqRes.message);

    // 2. ID uniqueness
    //    2a. session/batch check: IDs already used inside this batch
    //    2b. persistent check: files already on disk for this type
    if (const Value* known = req.Find("knownIds")) {
        for (const Value& k : known->AsArray()) {
            if (k.AsString() == idChk) {
                Value r = ErrorResponse(
                    validation::kDuplicateId,
                    "The ID \"" + idChk + "\" is already in use.\n\n"
                    "A file with this ID already exists on disk.\n\n"
                    "Please enter a different ID.");
                r.Set("duplicateId", idChk);
                r.Set("reason", "batch");
                return r;
            }
        }
    }

    if (!outputRoot.empty() && outputpaths::IDExistsOnDisk(outputRoot, type, idChk)) {
        Value r = ErrorResponse(
            validation::kDuplicateId,
            "The ID \"" + idChk + "\" is already in use.\n\n"
            "A file with this ID already exists on disk.\n\n"
            "Please enter a different ID.");
        r.Set("duplicateId", idChk);
        r.Set("reason", "filesystem");
        return r;
    }

    // 3. numeric validation + "N/A" defaults
    QRObject obj;
    validation::Result res = validation::NormalizeAndValidate(type, raw, obj);
    if (!res.ok) return ErrorResponse(res.code, res.message);

    Value r = OkResponse();
    r.Set("object", ObjectToJson(obj));
    return r;
}

static Value CmdExists(const Value& req) {
    std::string type = req.GetString("type");
    std::string id   = req.GetString("id");
    std::string root = req.GetString("outputRoot");

    Value r = OkResponse();
    if (!outputpaths::IsValidType(type)) {
        r.Set("exists", false);
        r.Set("reason", "bad_type");
        return r;
    }
    bool exists = !root.empty() && outputpaths::IDExistsOnDisk(root, type, id);
    r.Set("exists", exists);
    r.Set("stlRelativePath", outputpaths::STLRelativePath(type, id));
    r.Set("pngRelativePath", outputpaths::PNGRelativePath(type, id));
    return r;
}

static Value CmdPaths(const Value& req) {
    std::string type = req.GetString("type");
    std::string id   = req.GetString("id");
    Value r = OkResponse();
    r.Set("stlRelativePath", outputpaths::STLRelativePath(type, id));
    r.Set("pngRelativePath", outputpaths::PNGRelativePath(type, id));
    r.Set("folder",          outputpaths::FolderRelativePath(type));
    return r;
}

// Builds payload/matrix/mesh for one object and appends its report to `items`.
static bool GenerateInto(QRObject& obj,
                         bool includeMatrix,
                         const std::string& meshDir,
                         Value& itemOut,
                         std::string& error)
{
    if (!engine::GenerateObject(obj, error)) return false;

    Value item = Value::MakeObject();
    item.Set("index", 0);
    item.Set("id", obj.id);
    item.Set("type", obj.type);
    item.Set("payload", obj.payload);
    item.Set("triangleCount", (long long)obj.mesh.triangles.size());
    item.Set("matrixSize", (long long)obj.qrMatrix.size());
    item.Set("stlRelativePath", outputpaths::STLRelativePath(obj.type, obj.id));
    item.Set("pngRelativePath", outputpaths::PNGRelativePath(obj.type, obj.id));

    if (includeMatrix) {
        Value rows = Value::MakeArray();
        for (const auto& row : obj.qrMatrix) {
            Value r = Value::MakeArray();
            for (bool cell : row) r.Push(cell ? 1 : 0);
            rows.Push(r);
        }
        item.Set("matrix", rows);
    }

    if (!meshDir.empty()) {
        // The preview cache is keyed by type + id so a Holder and a Tool that
        // share an ID cannot clobber each other's mesh (they are separate
        // objects on disk, with separate output folders).
        std::filesystem::path mf =
            std::filesystem::path(meshDir) / (obj.type + "_" + obj.id + ".mesh");
        if (engine::WriteFloatBufferFile(obj.mesh, mf)) {
            item.Set("meshFile", mf.string());
        }
    }

    itemOut = item;
    return true;
}

static Value CmdGenerate(const Value& req) {
    const Value* items = req.Find("items");
    if (!items || !items->IsArray()) {
        return ErrorResponse("bad_request", "Missing 'items' array.");
    }

    bool includeMatrix = req.GetBool("includeMatrix", true);
    std::string meshDir = req.GetString("meshDir");

    Value out = Value::MakeObject();
    Value arr = Value::MakeArray();

    int index = 0;
    for (const Value& iv : items->AsArray()) {
        QRObject obj = ObjectFromJson(&iv);
        obj.sizeMM  = (float)req.GetNumber("sizeMM", 9.0);
        obj.depthMM = (float)req.GetNumber("depthMM", 0.8);

        Value item;
        std::string error;
        if (!GenerateInto(obj, includeMatrix, meshDir, item, error)) {
            Value r = ErrorResponse("generate_failed", error);
            r.Set("index", index);
            r.Set("id", obj.id);
            return r;
        }
        item.Set("index", index);
        arr.Push(item);
        index++;
    }

    out.Set("ok", true);
    out.Set("code", validation::kOk);
    out.Set("message", "");
    out.Set("items", arr);
    return out;
}

static Value CmdMesh(const Value& req) {
    QRObject obj = ObjectFromJson(req.Find("item"));
    if (obj.sizeMM  == 0.0f) obj.sizeMM  = (float)req.GetNumber("sizeMM", 9.0);
    if (obj.depthMM == 0.0f) obj.depthMM = (float)req.GetNumber("depthMM", 0.8);

    std::string error;
    if (!engine::GenerateObject(obj, error)) {
        return ErrorResponse("generate_failed", error);
    }

    Value r = OkResponse();
    r.Set("id", obj.id);
    r.Set("payload", obj.payload);
    r.Set("triangleCount", (long long)obj.mesh.triangles.size());
    r.Set("vertexCount", (long long)(obj.mesh.triangles.size() * 3));
    r.Set("floatsPerVertex", 9);
    r.Set("sizeMM", (double)obj.sizeMM);
    r.Set("depthMM", (double)obj.depthMM);
    r.Set("data", engine::Base64Encode(engine::MeshToFloatBuffer(obj.mesh)));
    return r;
}

static Value CmdExport(const Value& req) {
    std::string kind = req.GetString("kind");   // "stl" | "png"
    std::string root = req.GetString("outputRoot");
    bool overwrite   = req.GetBool("overwrite", false);

    QRObject obj = ObjectFromJson(req.Find("item"));
    if (!outputpaths::IsValidType(obj.type) || obj.id.empty()) {
        return ErrorResponse(engine::kBadType, "Export requires a valid type and id.");
    }
    if (root.empty()) {
        return ErrorResponse("bad_request", "Missing 'outputRoot'.");
    }

    if (obj.sizeMM  == 0.0f) obj.sizeMM  = (float)req.GetNumber("sizeMM", 9.0);
    if (obj.depthMM == 0.0f) obj.depthMM = (float)req.GetNumber("depthMM", 0.8);

    // Regenerate the matrix/mesh from the stored fields — deterministic and
    // identical to what GenerateAllQRs() produced at generation time.
    std::string error;
    if (!engine::GenerateObject(obj, error)) {
        return ErrorResponse("generate_failed", error);
    }

    bool isPNG = (kind == "png");
    if (!isPNG && kind != "stl") {
        return ErrorResponse("bad_request", "kind must be \"stl\" or \"png\".");
    }

    std::filesystem::path outPath =
        isPNG ? outputpaths::PNGAbsolutePath(root, obj.type, obj.id)
              : outputpaths::STLAbsolutePath(root, obj.type, obj.id);

    std::string relative = isPNG ? outputpaths::PNGRelativePath(obj.type, obj.id)
                                 : outputpaths::STLRelativePath(obj.type, obj.id);

    // Desktop app: the overwrite prompt is only ever raised when the file
    // actually exists on disk (TryExportCurrentSTL / TryExportCurrentPNG).
    std::error_code ec;
    if (!overwrite && std::filesystem::exists(outPath, ec)) {
        Value r = Value::MakeObject();
        r.Set("ok", false);
        r.Set("code", "file_exists");
        r.Set("message", "");
        r.Set("path", outPath.string());
        r.Set("relativePath", relative);
        r.Set("bytes", (long long)std::filesystem::file_size(outPath, ec));
        return r;
    }

    // "Overwrite" in the desktop popup did fs::remove() then re-exported.
    if (overwrite) {
        std::error_code rmEc;
        std::filesystem::remove(outPath, rmEc);
    }

    engine::ExportResult res =
        isPNG ? engine::ExportPNG(obj, outPath,
                                  req.GetInt("scale", 10),
                                  req.GetInt("quietZone", 4))
              : engine::ExportSTL(obj, outPath);

    if (!res.ok) {
        Value r = ErrorResponse(res.code, res.message);
        r.Set("path", res.path.string());
        r.Set("relativePath", relative);
        return r;
    }

    Value r = OkResponse();
    r.Set("kind", kind);
    r.Set("path", res.path.string());
    r.Set("relativePath", relative);
    r.Set("bytes", (long long)res.bytes);
    r.Set("payload", obj.payload);
    r.Set("triangleCount", (long long)obj.mesh.triangles.size());
    return r;
}

// ─────────────────────────────────────────────
//  Dispatch
// ─────────────────────────────────────────────

int main(int argc, char** argv) {
    std::string input;
    bool pretty = false;
    bool haveFile = false;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--in" && i + 1 < argc) {
            std::ifstream f(argv[++i], std::ios::binary);
            if (!f) {
                std::cout << ErrorResponse("bad_request", "Cannot read --in file").Dump() << std::endl;
                return 2;
            }
            std::stringstream ss; ss << f.rdbuf();
            input = ss.str();
            haveFile = true;
        } else if (a == "--pretty") {
            pretty = true;
        } else if (a == "--version") {
            std::cout << kVersion << std::endl;
            return 0;
        } else if (a == "--help" || a == "-h") {
            std::cout << "qr_engine " << kVersion
                      << "\nusage: qr_engine [--in FILE] [--pretty]  (JSON request on stdin)\n"
                      << "commands: ping validate generate export mesh exists paths\n";
            return 0;
        }
    }

    if (!haveFile) {
        std::stringstream ss;
        ss << std::cin.rdbuf();
        input = ss.str();
    }

    if (input.find_first_not_of(" \t\r\n") == std::string::npos) {
        std::cout << ErrorResponse("bad_request", "Empty request").Dump() << std::endl;
        return 2;
    }

    Value req;
    try {
        req = qrjson::Parse(input);
    } catch (const std::exception& e) {
        std::cout << ErrorResponse("bad_request", std::string("Malformed JSON: ") + e.what()).Dump()
                  << std::endl;
        return 2;
    }

    std::string cmd = req.GetString("cmd");
    Value resp;

    try {
        if      (cmd == "ping")     resp = CmdPing(req);
        else if (cmd == "validate") resp = CmdValidate(req);
        else if (cmd == "generate") resp = CmdGenerate(req);
        else if (cmd == "export")   resp = CmdExport(req);
        else if (cmd == "mesh")     resp = CmdMesh(req);
        else if (cmd == "exists")   resp = CmdExists(req);
        else if (cmd == "paths")    resp = CmdPaths(req);
        else resp = ErrorResponse("unknown_command", "Unknown cmd: '" + cmd + "'");
    } catch (const std::exception& e) {
        resp = ErrorResponse("engine_exception", e.what());
    }

    std::cout << resp.Dump(pretty ? 2 : -1) << std::endl;
    return resp.GetBool("ok") ? 0 : 1;
}
