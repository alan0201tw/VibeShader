#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace graphics {

struct MeshTriangle {
  glm::vec3 v0;
  float pad0 = 0.0f;
  glm::vec3 v1;
  float pad1 = 0.0f;
  glm::vec3 v2;
  float pad2 = 0.0f;
  glm::vec3 n0;
  float pad3 = 0.0f;
  glm::vec3 n1;
  float pad4 = 0.0f;
  glm::vec3 n2;
  float pad5 = 0.0f;
};

static_assert(sizeof(MeshTriangle) == sizeof(float) * 24,
              "MeshTriangle must match GLSL std430 layout");

// Loads triangles from an OBJ file. Returns empty on failure.
std::vector<MeshTriangle> LoadMeshFromObj(const std::string& filename);

}  // namespace graphics
