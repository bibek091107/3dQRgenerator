#pragma once

// Engine orchestration: payload -> QR matrix -> 3D mesh -> STL / PNG files.
//
// Mirrors App::GenerateAllQRs(), App::DoExportSTL() and App::DoExportPNG()
// from the original 3DQRGenerator/src/App.cpp, with every UI concern removed.

#include "Mesh.h"
#include "QRObject.h"
#include <string>
#include <filesystem>

namespace engine {

// Error codes returned by the service (mirrors the desktop app's failure paths).
inline constexpr const char* kNoMatrix     = "no_matrix";      // PNG export before QR generation
inline constexpr const char* kDirFailed    = "dir_failed";     // could not create output dir
inline constexpr const char* kWriteFailed  = "write_failed";   // writer returned false
inline constexpr const char* kVerifyFailed = "verify_failed";  // missing / empty after write
inline constexpr const char* kBadType      = "bad_type";       // type is neither Holder nor Tool

struct ExportResult {
    bool                 ok      = false;
    std::string          code;          // empty on success, else one of the k* codes
    std::string          message;       // exact text for the "Export Failed" popup
    std::filesystem::path path;         // absolute path written (or that would be written)
    unsigned long long   bytes    = 0;  // size on disk after a successful write
};

// Build payload + QR matrix + 3D mesh and pre-compute the canonical paths.
// Equivalent to App::GenerateAllQRs() for a single object. `obj` is updated
// in place; on failure the error message is returned in `error`.
bool GenerateObject(QRObject& obj, std::string& error);

// Export the binary STL for an object that already has a mesh.
// Mirrors App::DoExportSTL(), including the post-write existence/size check.
ExportResult ExportSTL(const QRObject& obj, const std::filesystem::path& outPath);

// Export the QR PNG for an object that already has a matrix.
// Mirrors App::DoExportPNG(). `scale`/`quietZone` default to the values the
// desktop app always used (10 px per module, 4-module quiet zone).
ExportResult ExportPNG(const QRObject& obj, const std::filesystem::path& outPath,
                       int scale = 10, int quietZone = 4);

// ── 3D preview bridge ─────────────────────────────────────────────
// The web client renders the preview with Three.js. To guarantee it shows the
// exact same geometry as the desktop OpenGL preview, the engine serializes the
// mesh using the same 9-floats-per-vertex layout Renderer::SetMesh() used:
//   px, py, pz, nx, ny, nz, cr, cg, cb
std::string MeshToFloatBuffer(const Mesh& mesh);
std::string Base64Encode(const std::string& raw);
bool WriteFloatBufferFile(const Mesh& mesh, const std::filesystem::path& file);

// Ensure output/holder and output/tools exist under `root` (App::Run()).
void EnsureOutputDirs(const std::filesystem::path& root);

} // namespace engine
