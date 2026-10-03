#pragma once

#include <vector>

struct Vertex {
    float x;
    float y;
    float z;
};

struct Triangle {
    Vertex normal;
    Vertex v1;
    Vertex v2;
    Vertex v3;
    Vertex color; // r, g, b
};

struct Mesh {
    std::vector<Triangle> triangles;
};
