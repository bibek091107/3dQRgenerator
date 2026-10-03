#include "EngineService.h"

#include "QRGenerator.h"
#include "MeshBuilder.h"
#include "STLWriter.h"
#include "PNGWriter.h"
#include "PayloadBuilder.h"
#include "OutputPaths.h"

#include <fstream>
#include <cstring>

namespace engine {

// ─────────────────────────────────────────────
//  Generation — App::GenerateAllQRs()
// ─────────────────────────────────────────────

bool GenerateObject(QRObject& obj, std::string& error) {
    error.clear();

    if (!outputpaths::IsValidType(obj.type)) {
        error = "Type must be either \"Holder\" or \"Tool\".";
        return false;
    }

    // 1. Plain-text payload
    obj.payload = PayloadBuilder::BuildPayload(obj);

    // 2. QR matrix generated ONCE — shared by the PNG and the 3D mesh
    obj.qrMatrix = QRGenerator::GenerateMatrix(obj.payload);
    if (obj.qrMatrix.empty()) {
        error = "QR generation produced an empty matrix.";
        return false;
    }

    // 3. 3D mesh from that same matrix
    obj.mesh = MeshBuilder::BuildQRMesh(obj.qrMatrix, obj);
    if (obj.mesh.triangles.empty()) {
        error = "3D mesh generation produced no triangles.";
        return false;
    }

    // 4. Canonical paths (relative form, as shown in the desktop preview panel)
    obj.stlPath = std::filesystem::path(outputpaths::STLRelativePath(obj.type, obj.id));
    obj.pngPath = std::filesystem::path(outputpaths::PNGRelativePath(obj.type, obj.id));

    return true;
}

// ─────────────────────────────────────────────
//  STL export — App::DoExportSTL()
// ─────────────────────────────────────────────

ExportResult ExportSTL(const QRObject& obj, const std::filesystem::path& outPath) {
    ExportResult r;
    r.path = outPath;

    std::error_code ec;
    std::filesystem::create_directories(outPath.parent_path(), ec);
    if (ec) {
        r.code = kDirFailed;
        r.message = "STL Export Failed\n\nCould not create output directory.\n\nPath:\n" +
                    outPath.string();
        return r;
    }

    if (!STLWriter::WriteBinarySTL(obj.mesh, outPath.string())) {
        r.code = kWriteFailed;
        r.message = "STL Export Failed\n\nCould not write file.\n\nPath:\n" + outPath.string();
        return r;
    }

    // Filesystem is authoritative — verify the file really landed
    if (!std::filesystem::exists(outPath) ||
        std::filesystem::file_size(outPath) == 0) {
        r.code = kVerifyFailed;
        r.message = "STL Export Failed\n\nFile not created or empty.\n\nPath:\n" + outPath.string();
        return r;
    }

    r.ok = true;
    r.bytes = (unsigned long long)std::filesystem::file_size(outPath);
    return r;
}

// ─────────────────────────────────────────────
//  PNG export — App::DoExportPNG()
// ─────────────────────────────────────────────

ExportResult ExportPNG(const QRObject& obj, const std::filesystem::path& outPath,
                       int scale, int quietZone)
{
    ExportResult r;
    r.path = outPath;

    if (obj.qrMatrix.empty()) {
        r.code = kNoMatrix;
        r.message = "PNG Export Failed\n\nNo QR matrix available.\nGenerate the QR first.";
        return r;
    }

    std::error_code ec;
    std::filesystem::create_directories(outPath.parent_path(), ec);
    if (ec) {
        r.code = kDirFailed;
        r.message = "PNG Export Failed\n\nCould not create output directory.\n\nPath:\n" +
                    outPath.string();
        return r;
    }

    if (!PNGWriter::WriteQRPNG(outPath.string(), obj.qrMatrix, scale, quietZone)) {
        r.code = kWriteFailed;
        r.message = "PNG Export Failed\n\nCould not write PNG file.\n\nPath:\n" + outPath.string();
        return r;
    }

    if (!std::filesystem::exists(outPath) ||
        std::filesystem::file_size(outPath) == 0) {
        r.code = kVerifyFailed;
        r.message = "PNG Export Failed\n\nFile not created or empty.\n\nPath:\n" + outPath.string();
        return r;
    }

    r.ok = true;
    r.bytes = (unsigned long long)std::filesystem::file_size(outPath);
    return r;
}

// ─────────────────────────────────────────────
//  Preview bridge
// ─────────────────────────────────────────────

std::string MeshToFloatBuffer(const Mesh& mesh) {
    // 9 floats per vertex — identical to Renderer::SetMesh()'s interleaved layout
    std::string buf;
    buf.reserve(mesh.triangles.size() * 3 * 9 * sizeof(float));

    auto push = [&buf](float f) {
        buf.append(reinterpret_cast<const char*>(&f), sizeof(float));
    };

    for (const auto& tri : mesh.triangles) {
        const Vertex* verts[3] = { &tri.v1, &tri.v2, &tri.v3 };
        for (const Vertex* v : verts) {
            push(v->x); push(v->y); push(v->z);
            push(tri.normal.x); push(tri.normal.y); push(tri.normal.z);
            push(tri.color.x); push(tri.color.y); push(tri.color.z);
        }
    }
    return buf;
}

static const char* kB64 =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string Base64Encode(const std::string& raw) {
    std::string out;
    out.reserve(((raw.size() + 2) / 3) * 4);

    size_t i = 0;
    const unsigned char* p = reinterpret_cast<const unsigned char*>(raw.data());
    while (i + 2 < raw.size()) {
        unsigned int n = (p[i] << 16) | (p[i + 1] << 8) | p[i + 2];
        out.push_back(kB64[(n >> 18) & 63]);
        out.push_back(kB64[(n >> 12) & 63]);
        out.push_back(kB64[(n >>  6) & 63]);
        out.push_back(kB64[ n        & 63]);
        i += 3;
    }
    if (i + 1 == raw.size()) {
        unsigned int n = p[i] << 16;
        out.push_back(kB64[(n >> 18) & 63]);
        out.push_back(kB64[(n >> 12) & 63]);
        out.push_back('=');
        out.push_back('=');
    } else if (i + 2 == raw.size()) {
        unsigned int n = (p[i] << 16) | (p[i + 1] << 8);
        out.push_back(kB64[(n >> 18) & 63]);
        out.push_back(kB64[(n >> 12) & 63]);
        out.push_back(kB64[(n >>  6) & 63]);
        out.push_back('=');
    }
    return out;
}

bool WriteFloatBufferFile(const Mesh& mesh, const std::filesystem::path& file) {
    std::string data = MeshToFloatBuffer(mesh);
    std::error_code ec;
    std::filesystem::create_directories(file.parent_path(), ec);
    std::ofstream out(file, std::ios::binary);
    if (!out.is_open()) return false;
    out.write(data.data(), (std::streamsize)data.size());
    out.close();
    return out.good();
}

void EnsureOutputDirs(const std::filesystem::path& root) {
    std::error_code ec;
    std::filesystem::create_directories(root / "output" / "holder", ec);
    std::filesystem::create_directories(root / "output" / "tools", ec);
}

} // namespace engine
