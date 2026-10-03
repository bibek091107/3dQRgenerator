#include "MeshBuilder.h"
#include <cmath>

Mesh MeshBuilder::BuildQRMesh(const std::vector<std::vector<bool>>& qrMatrix, const QRObject& obj) {
    Mesh mesh;

    int qrSize = qrMatrix.size();
    if (qrSize == 0) return mesh;

    // A standard quiet zone is 4 modules, but we'll use 2 for physical compactness, or user-defined.
    int quietZone = 2; 
    int totalModules = qrSize + 2 * quietZone;

    float moduleSize = obj.sizeMM / totalModules;
    float baseThickness = 0.5f; // Hardcoded base thickness as requested
    float qrDepth = obj.depthMM;

    auto addQuad = [&](Vertex v1, Vertex v2, Vertex v3, Vertex v4, Vertex normal, Vertex color) {
        mesh.triangles.push_back({normal, v1, v2, v3, color});
        mesh.triangles.push_back({normal, v1, v3, v4, color});
    };

    // Build the Base Plate
    float baseWidth = obj.sizeMM;
    float baseHeight = obj.sizeMM;

    Vertex b1 = {0.0f, 0.0f, 0.0f};
    Vertex b2 = {baseWidth, 0.0f, 0.0f};
    Vertex b3 = {baseWidth, baseHeight, 0.0f};
    Vertex b4 = {0.0f, baseHeight, 0.0f};

    Vertex t1 = {0.0f, 0.0f, baseThickness};
    Vertex t2 = {baseWidth, 0.0f, baseThickness};
    Vertex t3 = {baseWidth, baseHeight, baseThickness};
    Vertex t4 = {0.0f, baseHeight, baseThickness};

    // Base bottom (normal 0,0,-1)
    addQuad(b1, b4, b3, b2, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f, 1.0f});
    // Base top (normal 0,0,1)
    addQuad(t1, t2, t3, t4, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f, 1.0f});
    // Front, Back, Left, Right of base
    addQuad(b1, b2, t2, t1, {0.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f});
    addQuad(b3, b4, t4, t3, {0.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 1.0f});
    addQuad(b4, b1, t1, t4, {-1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f});
    addQuad(b2, b3, t3, t2, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f});

    // Build QR Modules
    for (int y = 0; y < qrSize; y++) {
        for (int x = 0; x < qrSize; x++) {
            if (qrMatrix[y][x]) {
                float startX = (x + quietZone) * moduleSize;
                // QR coordinate Y is top-down usually, let's map to physical Y (which is also usually Y-up in OpenGL)
                // We'll just map it directly. If it gets flipped, we'll fix it later.
                float startY = (y + quietZone) * moduleSize;

                float ex = startX + moduleSize;
                float ey = startY + moduleSize;
                float ez = baseThickness + qrDepth;

                Vertex mb1 = {startX, startY, baseThickness};
                Vertex mb2 = {ex, startY, baseThickness};
                Vertex mb3 = {ex, ey, baseThickness};
                Vertex mb4 = {startX, ey, baseThickness};

                Vertex mt1 = {startX, startY, ez};
                Vertex mt2 = {ex, startY, ez};
                Vertex mt3 = {ex, ey, ez};
                Vertex mt4 = {startX, ey, ez};

                // Module top
                addQuad(mt1, mt2, mt3, mt4, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f});
                // Module sides
                addQuad(mb1, mb2, mt2, mt1, {0.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
                addQuad(mb3, mb4, mt4, mt3, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
                addQuad(mb4, mb1, mt1, mt4, {-1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
                addQuad(mb2, mb3, mt3, mt2, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f});
            }
        }
    }

    return mesh;
}
