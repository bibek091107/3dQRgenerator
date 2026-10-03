#pragma once

#include "Mesh.h"
#include "QRObject.h"
#include <vector>

class MeshBuilder {
public:
    // Builds a 3D mesh from a QR matrix using the specified physical dimensions
    // Includes a base and a quiet zone.
    static Mesh BuildQRMesh(const std::vector<std::vector<bool>>& qrMatrix, const QRObject& obj);
};
