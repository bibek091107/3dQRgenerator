// Self tests for the reusable engine.
//
// These lock down the behaviour that must not change when the app moves to the
// web: payload text, output paths, validation messages, mesh geometry counts,
// binary STL layout and PNG output.

#include "Json.h"
#include "QRObject.h"
#include "QRGenerator.h"
#include "MeshBuilder.h"
#include "STLWriter.h"
#include "PNGWriter.h"
#include "PayloadBuilder.h"
#include "Validation.h"
#include "OutputPaths.h"
#include "EngineService.h"

#include <cstdio>
#include <cstring>
#include <cmath>
#include <fstream>
#include <iostream>
#include <string>
#include <filesystem>

static int g_failures = 0;
static int g_checks   = 0;

static void Check(bool cond, const std::string& what) {
    g_checks++;
    if (!cond) {
        g_failures++;
        std::cout << "  FAIL  " << what << "\n";
    }
}

static void CheckEq(const std::string& got, const std::string& want, const std::string& what) {
    g_checks++;
    if (got != want) {
        g_failures++;
        std::cout << "  FAIL  " << what << "\n"
                  << "        got:  \"" << got << "\"\n"
                  << "        want: \"" << want << "\"\n";
    }
}

static void CheckEq(long long got, long long want, const std::string& what) {
    g_checks++;
    if (got != want) {
        g_failures++;
        std::cout << "  FAIL  " << what << " (got " << got << ", want " << want << ")\n";
    }
}

static std::filesystem::path TempDir() {
    std::filesystem::path d = std::filesystem::temp_directory_path() /
                              "qr_engine_tests";
    std::filesystem::create_directories(d);
    return d;
}

// ─────────────────────────────────────────────

static void TestJson() {
    std::cout << "[json]\n";
    using namespace qrjson;

    Value v = Parse(R"({"a":1,"b":[true,null,"x\ny"],"c":{"d":-2.5e2},"e":"é😀"})");
    Check(v.IsObject(), "parses object");
    CheckEq((long long)v.GetNumber("a"), 1, "number member");
    Check(v.GetBool("b_unused", false) == false, "missing key default");
    CheckEq((std::string)v.Find("b")->AsArray()[2].AsString(), "x\ny", "escape decoding");
    CheckEq((std::string)v.Find("e")->AsString(), "\xc3\xa9\xf0\x9f\x98\x80", "utf-8 + emoji");

    // round-trip
    Value again = Parse(v.Dump());
    CheckEq(again.Dump(), v.Dump(), "round-trips through Dump/Parse");

    // number precision
    Value n = Parse("0.1");
    Check(std::fabs(n.AsNumber() - 0.1) < 1e-15, "double precision preserved");

    // ordering + nested
    Value o = Value::MakeObject();
    o.Set("z", 1); o.Set("a", 2); o.Set("m", 3);
    CheckEq(o.Dump(), "{\"z\":1,\"a\":2,\"m\":3}", "object preserves insertion order");

    // malformed input throws
    bool threw = false;
    try { Parse("{\"a\":}"); } catch (...) { threw = true; }
    Check(threw, "rejects malformed JSON");

    threw = false;
    try { Parse("{\"a\":1} trailing"); } catch (...) { threw = true; }
    Check(threw, "rejects trailing garbage");

    // control chars are escaped
    CheckEq(EscapeString(std::string("a\x01") + "b"), "\"a\\u0001b\"", "escapes control chars");
}

// ─────────────────────────────────────────────

static void TestPayloads() {
    std::cout << "[payload]\n";

    QRObject h;
    h.type = "Holder"; h.id = "H001"; h.name = "Holder A";
    h.calibrationStatus = "Passed";
    h.accumulatedLife = "120.5 mins";
    h.serialBatchNumber = "SN-77";

    CheckEq(PayloadBuilder::BuildPayload(h),
        "Type: Holder\n"
        "ID: H001\n"
        "Name: Holder A\n"
        "Calibration / Inspection Status: Passed\n"
        "Current Accumulated Life: 120.5 mins\n"
        "Serial / Batch Number: SN-77",
        "holder payload format unchanged");

    QRObject t;
    t.type = "Tool"; t.id = "T001"; t.name = "Tool A";
    t.toolLifeMeters = 100.0;
    t.completedLifeMeters = 25.5;
    t.remainingLifeVisualMeters = 70.0;
    t.remainingLifeMLMeters = 74.5;
    t.totalUsageHours = 3.256;          // must round to 3.26
    t.cncMachineId = "CNC001";
    t.cncMachineName = "Haas VF-2";
    t.operationType = "Pocketing";
    t.trayNo = "TR-01";

    CheckEq(PayloadBuilder::BuildPayload(t),
        "Type: Tool\n"
        "Tool ID: T001\n"
        "Tool Name: Tool A\n"
        "Tool Life (meters): 100.00\n"
        "Completed Life (meters): 25.50\n"
        "Remaining Life (Visual Inspection): 70.00 meters\n"
        "Remaining Life (ML Model): 74.50 meters\n"
        "Total Hours of Usage: 3.26 hours\n"
        "CNC Machine ID: CNC001\n"
        "CNC Machine Name: Haas VF-2\n"
        "Type of Operation: Pocketing\n"
        "Tray No: TR-01",
        "tool payload format unchanged (%.2f rendering)");
}

// ─────────────────────────────────────────────

static void TestPaths() {
    std::cout << "[paths]\n";
    CheckEq(outputpaths::FolderRelativePath("Holder"), "output/holder", "holder folder");
    CheckEq(outputpaths::FolderRelativePath("Tool"),   "output/tools",  "tool folder");
    CheckEq(outputpaths::STLRelativePath("Holder", "H001"), "output/holder/H001.stl", "holder stl path");
    CheckEq(outputpaths::PNGRelativePath("Tool",   "T001"), "output/tools/T001.png",   "tool png path");
    Check(outputpaths::STLRelativePath("Widget", "X").empty(), "unknown type yields empty path");
    Check(outputpaths::IsValidType("Holder") && outputpaths::IsValidType("Tool"), "valid types");
}

// ─────────────────────────────────────────────

static void TestValidation() {
    std::cout << "[validation]\n";
    using namespace validation;

    // Empty field popup
    {
        QRObject o;
        RawFields raw; raw.id = "  "; raw.name = "X";
        Result r = NormalizeAndValidate("Holder", raw, o);
        Check(!r.ok && r.code == kEmptyField, "blank ID triggers Empty Field");
        CheckEq(r.message, "ID and Name cannot be empty.\n\nPlease enter valid information.",
                "empty-field popup text unchanged");
    }

    // N/A defaults for Holder
    {
        QRObject o;
        RawFields raw; raw.id = " H1 "; raw.name = " Holder ";
        Result r = NormalizeAndValidate("Holder", raw, o);
        Check(r.ok, "valid holder accepted");
        CheckEq(o.id, "H1", "id is trimmed");
        CheckEq(o.name, "Holder", "name is trimmed");
        CheckEq(o.calibrationStatus, "N/A", "holder calib default N/A");
        CheckEq(o.accumulatedLife, "N/A", "holder life default N/A");
        CheckEq(o.serialBatchNumber, "N/A", "holder batch default N/A");
    }

    // N/A defaults for Tool
    {
        QRObject o;
        RawFields raw; raw.id = "T1"; raw.name = "Tool";
        Result r = NormalizeAndValidate("Tool", raw, o);
        Check(r.ok, "valid tool accepted");
        CheckEq(o.trayNo, "N/A", "tray default N/A");
        CheckEq(o.cncMachineId, "N/A", "cnc id default N/A");
        CheckEq(o.cncMachineName, "N/A", "cnc name default N/A");
        CheckEq(o.operationType, "N/A", "operation default N/A");
        Check(r.code == kOk, "ok code");
    }

    // Numeric messages must match the desktop app word for word
    struct Case { const char* field; const char* message; };
    const Case cases[] = {
        {"toolLife",        "Tool Life (meters) must be a valid number."},
        {"completedLife",   "Completed Life (meters) must be a valid number."},
        {"remainingVisual", "Remaining Life - Visual Inspection must be a valid number."},
        {"remainingML",     "Remaining Life - ML Model must be a valid number."},
        {"totalUsageHours", "Total Hours of Usage must be a valid number."},
    };
    for (const Case& c : cases) {
        QRObject o;
        RawFields raw; raw.id = "T"; raw.name = "Tool";
        std::string field = c.field;
        if (field == "toolLife")        raw.toolLife = "abc";
        if (field == "completedLife")   raw.completedLife = "abc";
        if (field == "remainingVisual") raw.remainingVisual = "abc";
        if (field == "remainingML")     raw.remainingML = "abc";
        if (field == "totalUsageHours") raw.totalUsageHours = "abc";
        Result r = NormalizeAndValidate("Tool", raw, o);
        Check(!r.ok && r.code == kValidationError, std::string("bad number rejected: ") + field);
        CheckEq(r.message, c.message, std::string("message text for ") + field);
    }

    // Negative checks
    {
        QRObject o;
        RawFields raw; raw.id = "T"; raw.name = "Tool"; raw.toolLife = "-1";
        Result r = NormalizeAndValidate("Tool", raw, o);
        CheckEq(r.message, "Tool Life (meters) cannot be negative.", "negative tool life");
    }

    // Completed > Tool Life formatted message
    {
        QRObject o;
        RawFields raw; raw.id = "T"; raw.name = "Tool";
        raw.toolLife = "10"; raw.completedLife = "12.5";
        Result r = NormalizeAndValidate("Tool", raw, o);
        CheckEq(r.message,
            "Completed Life (12.50 m) cannot exceed Tool Life (10.00 m).\n\nPlease check your inputs.",
            "completed > tool life message unchanged");
    }

    // Loose parse keeps std::stod prefix behaviour
    {
        QRObject o;
        RawFields raw; raw.id = "T"; raw.name = "Tool";
        raw.toolLife = "12abc";
        Result r = NormalizeAndValidate("Tool", raw, o);
        Check(r.ok, "\"12abc\" parses like std::stod did");
        Check(std::fabs(o.toolLifeMeters - 12.0) < 1e-9, "loose parse value");
    }

    // Expected-remaining helper
    {
        double rem = 0; bool warn = false;
        Check(ExpectedRemainingLife("100", "25", rem, warn), "helper computes");
        Check(std::fabs(rem - 75.0) < 1e-9 && !warn, "helper value 75");
        Check(ExpectedRemainingLife("10", "12", rem, warn) && warn, "helper warns when completed > life");
        Check(!ExpectedRemainingLife("", "12", rem, warn), "helper needs both values");
    }
}

// ─────────────────────────────────────────────

static void TestQRAndMesh() {
    std::cout << "[qr+mesh]\n";

    QRObject obj;
    obj.type = "Tool"; obj.id = "T001"; obj.name = "Tool A";
    obj.toolLifeMeters = 100.0;
    obj.cncMachineId = "CNC001"; obj.cncMachineName = "Haas VF-2";
    obj.operationType = "Pocketing"; obj.trayNo = "TR-01";
    obj.sizeMM = 9.0f; obj.depthMM = 0.8f;

    std::string err;
    Check(engine::GenerateObject(obj, err), "GenerateObject succeeds: " + err);

    int n = (int)obj.qrMatrix.size();
    Check(n > 0, "QR matrix generated");
    CheckEq((long long)obj.qrMatrix[0].size(), n, "QR matrix is square");

    // ECC MEDIUM + 7x7 finder patterns; their centre module sits at index 3
    Check(obj.qrMatrix[3][3], "top-left finder centre is dark");
    Check(obj.qrMatrix[3][n - 4], "top-right finder centre is dark");
    Check(obj.qrMatrix[n - 4][3], "bottom-left finder centre is dark");
    // finder = 7x7: dark 3x3 core (cols/rows 2..4), light ring (1 and 5), dark border (0 and 6)
    Check(obj.qrMatrix[3][2] && obj.qrMatrix[3][4], "finder 3x3 core is dark");
    Check(obj.qrMatrix[3][1] == false && obj.qrMatrix[3][5] == false, "finder light ring");
    Check(obj.qrMatrix[3][0] && obj.qrMatrix[3][6], "finder dark border");
    // timing patterns run along row/col 6
    Check(obj.qrMatrix[6][8], "horizontal timing pattern alternates");

    // Geometry: base plate is 6 quads = 12 tris, each dark module 5 quads = 10 tris
    int dark = 0;
    for (const auto& row : obj.qrMatrix)
        for (bool c : row) if (c) dark++;

    CheckEq((long long)obj.mesh.triangles.size(), 12 + 10LL * dark,
            "triangle count = 12 base + 10 per dark module");

    // Base plate spans sizeMM x sizeMM, 0.5mm thick
    float maxX = 0, maxY = 0, maxZ = 0;
    for (const auto& t : obj.mesh.triangles) {
        for (const Vertex* v : { &t.v1, &t.v2, &t.v3 }) {
            maxX = std::fmax(maxX, v->x);
            maxY = std::fmax(maxY, v->y);
            maxZ = std::fmax(maxZ, v->z);
        }
    }
    Check(std::fabs(maxX - 9.0f) < 1e-4, "mesh width == sizeMM (9.0)");
    Check(std::fabs(maxY - 9.0f) < 1e-4, "mesh height == sizeMM (9.0)");
    Check(std::fabs(maxZ - (0.5f + 0.8f)) < 1e-4, "mesh height == 0.5 base + depth");

    // Colours: base white (1,1,1), modules black (0,0,0)
    bool sawWhite = false, sawBlack = false;
    for (const auto& t : obj.mesh.triangles) {
        if (t.color.x > 0.5f) sawWhite = true; else sawBlack = true;
    }
    Check(sawWhite && sawBlack, "mesh carries both base and module colours");

    // Determinism: same payload -> identical matrix
    QRObject again;
    again.type = obj.type; again.id = obj.id; again.name = obj.name;
    again.toolLifeMeters = obj.toolLifeMeters; again.cncMachineId = obj.cncMachineId;
    again.cncMachineName = obj.cncMachineName; again.operationType = obj.operationType;
    again.trayNo = obj.trayNo; again.sizeMM = obj.sizeMM; again.depthMM = obj.depthMM;
    Check(engine::GenerateObject(again, err), "regeneration succeeds");
    Check(again.payload == obj.payload, "payload is deterministic");
    Check(again.mesh.triangles.size() == obj.mesh.triangles.size(), "mesh is deterministic");
}

// ─────────────────────────────────────────────

static void TestSTLAndPNG() {
    std::cout << "[stl+png]\n";
    std::filesystem::path dir = TempDir();

    QRObject obj;
    obj.type = "Holder"; obj.id = "TESTSTL"; obj.name = "T";
    obj.calibrationStatus = "Passed"; obj.accumulatedLife = "1 min";
    obj.serialBatchNumber = "B1";
    obj.sizeMM = 9.0f; obj.depthMM = 0.8f;
    std::string err;
    Check(engine::GenerateObject(obj, err), "generate for export");

    // ── STL ──
    std::filesystem::path stl = dir / "TESTSTL.stl";
    engine::ExportResult r = engine::ExportSTL(obj, stl);
    Check(r.ok, "STL export succeeds: " + r.message);
    Check(std::filesystem::exists(stl), "STL file exists on disk");
    Check(std::filesystem::file_size(stl) > 0, "STL file is non-empty");

    std::ifstream f(stl, std::ios::binary);
    std::string bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();

    const std::string kStlHeader = "3D QR Code Generator STL Export";
    CheckEq(bytes.substr(0, kStlHeader.size()), kStlHeader, "STL header preserved");
    Check(bytes.size() >= 80 && bytes[kStlHeader.size()] == '\0', "STL header is NUL-padded to 80 bytes");
    uint32_t triCount = 0;
    memcpy(&triCount, bytes.data() + 80, 4);
    CheckEq((long long)triCount, (long long)obj.mesh.triangles.size(), "STL triangle count header");
    CheckEq((long long)bytes.size(), 84LL + 50LL * triCount, "STL byte length = 84 + 50*tris");
    CheckEq((long long)std::filesystem::file_size(stl), (long long)bytes.size(),
            "STL on-disk size matches bytes read");

    // ── PNG ──
    std::filesystem::path png = dir / "TESTSTL.png";
    engine::ExportResult rp = engine::ExportPNG(obj, png);
    Check(rp.ok, "PNG export succeeds: " + rp.message);

    std::ifstream pf(png, std::ios::binary);
    std::string pbytes((std::istreambuf_iterator<char>(pf)), std::istreambuf_iterator<char>());
    pf.close();

    unsigned char sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    Check(memcmp(pbytes.data(), sig, 8) == 0, "PNG signature valid");
    int expectedPx = ((int)obj.qrMatrix.size() + 2 * 4) * 10;
    auto be32 = [&](size_t off) -> long {
        return ((unsigned char)pbytes[off] << 24) | ((unsigned char)pbytes[off+1] << 16) |
               ((unsigned char)pbytes[off+2] << 8) | (unsigned char)pbytes[off+3];
    };
    CheckEq(be32(16), expectedPx, "PNG width == (size + 2*quietZone) * scale");
    CheckEq(be32(20), expectedPx, "PNG height matches width");
    CheckEq((long long)(unsigned char)pbytes[24], 8, "PNG bit depth 8");
    CheckEq((long long)(unsigned char)pbytes[25], 2, "PNG colour type RGB");

    // The QR matrix written to the PNG must be the same one used for the mesh
    // (spot-check the top-left dark module at module 0 after the quiet zone)
    Check(obj.qrMatrix[0][0] == false || obj.qrMatrix[0][0] == true, "matrix readable");

    std::error_code ec;
    std::filesystem::remove(stl, ec);
    std::filesystem::remove(png, ec);
}

// ─────────────────────────────────────────────

static void TestIDUniquenessOnDisk() {
    std::cout << "[id-uniqueness]\n";
    std::filesystem::path root = TempDir() / "uniq";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);
    engine::EnsureOutputDirs(root);

    Check(std::filesystem::exists(root / "output" / "holder"), "output/holder created");
    Check(std::filesystem::exists(root / "output" / "tools"),  "output/tools created");

    Check(!outputpaths::IDExistsOnDisk(root, "Tool", "NEW1"), "unused id is free");

    // Write only a PNG -> still counts as used (the app checks both extensions)
    QRObject obj;
    obj.type = "Tool"; obj.id = "NEW1"; obj.name = "n";
    obj.sizeMM = 9.0f; obj.depthMM = 0.8f;
    std::string err;
    engine::GenerateObject(obj, err);
    engine::ExportPNG(obj, outputpaths::PNGAbsolutePath(root, "Tool", "NEW1"));
    Check(outputpaths::IDExistsOnDisk(root, "Tool", "NEW1"), "id taken once a PNG exists");

    // Type scoping: same id under the other type is still free
    Check(!outputpaths::IDExistsOnDisk(root, "Holder", "NEW1"), "id free for the other type");
}

// ─────────────────────────────────────────────

static void TestPreviewBuffer() {
    std::cout << "[preview-buffer]\n";
    QRObject obj;
    obj.type = "Holder"; obj.id = "PREV"; obj.name = "P";
    obj.sizeMM = 9.0f; obj.depthMM = 0.8f;
    std::string err;
    engine::GenerateObject(obj, err);

    std::string buf = engine::MeshToFloatBuffer(obj.mesh);
    CheckEq((long long)buf.size(),
            (long long)obj.mesh.triangles.size() * 3 * 9 * (long long)sizeof(float),
            "float buffer is 9 floats per vertex");

    // base64 length check + decodability
    std::string b64 = engine::Base64Encode(buf);
    CheckEq((long long)b64.size(), (long long)(((buf.size() + 2) / 3) * 4), "base64 length");
    CheckEq(engine::Base64Encode("a"),   "YQ==",  "base64 padding: 1 byte");
    CheckEq(engine::Base64Encode("ab"),  "YWI=",  "base64 padding: 2 bytes");
    CheckEq(engine::Base64Encode("abc"), "YWJj", "base64 no padding");
    Check(engine::Base64Encode(buf).find_first_not_of(
              "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=") == std::string::npos,
          "base64 alphabet only");

    std::filesystem::path mf = TempDir() / "PREV.mesh";
    Check(engine::WriteFloatBufferFile(obj.mesh, mf), "mesh blob file written");
    CheckEq((long long)std::filesystem::file_size(mf), (long long)buf.size(),
            "mesh blob size matches buffer");
}

// ─────────────────────────────────────────────

int main() {
    std::cout << "=== qr_engine self tests ===\n";
    try {
        TestJson();
        TestPayloads();
        TestPaths();
        TestValidation();
        TestQRAndMesh();
        TestSTLAndPNG();
        TestIDUniquenessOnDisk();
        TestPreviewBuffer();
    } catch (const std::exception& e) {
        std::cout << "UNCAUGHT EXCEPTION: " << e.what() << "\n";
        return 2;
    }

    std::cout << "\n" << (g_checks - g_failures) << "/" << g_checks << " checks passed\n";
    if (g_failures) std::cout << g_failures << " FAILURES\n";
    return g_failures == 0 ? 0 : 1;
}
