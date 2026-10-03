#pragma once

#include "Mesh.h"
#include <string>

class STLWriter {
public:
    static bool WriteBinarySTL(const Mesh& mesh, const std::string& filepath);
};
