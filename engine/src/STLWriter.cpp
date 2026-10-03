#include "STLWriter.h"
#include <fstream>
#include <cstdint>

bool STLWriter::WriteBinarySTL(const Mesh& mesh, const std::string& filepath) {
    std::ofstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    // Write 80-byte header
    char header[80] = {0};
    std::string headerStr = "3D QR Code Generator STL Export";
    for(size_t i = 0; i < headerStr.size() && i < 80; i++) {
        header[i] = headerStr[i];
    }
    file.write(header, 80);

    // Write triangle count
    uint32_t triangleCount = mesh.triangles.size();
    file.write(reinterpret_cast<const char*>(&triangleCount), sizeof(triangleCount));

    // Write triangles
    for (const auto& tri : mesh.triangles) {
        // Normal
        file.write(reinterpret_cast<const char*>(&tri.normal.x), sizeof(float));
        file.write(reinterpret_cast<const char*>(&tri.normal.y), sizeof(float));
        file.write(reinterpret_cast<const char*>(&tri.normal.z), sizeof(float));

        // V1
        file.write(reinterpret_cast<const char*>(&tri.v1.x), sizeof(float));
        file.write(reinterpret_cast<const char*>(&tri.v1.y), sizeof(float));
        file.write(reinterpret_cast<const char*>(&tri.v1.z), sizeof(float));

        // V2
        file.write(reinterpret_cast<const char*>(&tri.v2.x), sizeof(float));
        file.write(reinterpret_cast<const char*>(&tri.v2.y), sizeof(float));
        file.write(reinterpret_cast<const char*>(&tri.v2.z), sizeof(float));

        // V3
        file.write(reinterpret_cast<const char*>(&tri.v3.x), sizeof(float));
        file.write(reinterpret_cast<const char*>(&tri.v3.y), sizeof(float));
        file.write(reinterpret_cast<const char*>(&tri.v3.z), sizeof(float));

        // Attribute byte count (0)
        uint16_t attr = 0;
        file.write(reinterpret_cast<const char*>(&attr), sizeof(attr));
    }

    file.close();
    return true;
}
