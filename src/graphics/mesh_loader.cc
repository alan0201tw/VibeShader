#include "graphics/mesh_loader.h"
#include <tiny_obj_loader.h>
#include <iostream>

namespace graphics {

namespace {

void SetTriangleVertex(MeshTriangle& tri, int vertex_index,
                       const glm::vec3& position,
                       const glm::vec3& normal) {
  switch (vertex_index) {
    case 0:
      tri.v0 = position;
      tri.n0 = normal;
      break;
    case 1:
      tri.v1 = position;
      tri.n1 = normal;
      break;
    case 2:
      tri.v2 = position;
      tri.n2 = normal;
      break;
  }
}

}  // namespace

std::vector<MeshTriangle> LoadMeshFromObj(const std::string& filename) {
  std::vector<MeshTriangle> tris;
  tinyobj::ObjReader reader;
  if (!reader.ParseFromFile(filename)) {
    std::cerr << "Failed to load OBJ: " << filename << "\n";
    return tris;
  }
  const auto& attrib = reader.GetAttrib();
  const auto& shapes = reader.GetShapes();
  for (const auto& shape : shapes) {
    size_t idx_offset = 0;
    for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
      int fv = shape.mesh.num_face_vertices[f];
      if (fv != 3) { idx_offset += fv; continue; } // Only triangles
      MeshTriangle tri;
      for (int v = 0; v < 3; v++) {
        const auto idx = shape.mesh.indices[idx_offset + v];
        auto vx = attrib.vertices[3 * idx.vertex_index + 0];
        auto vy = attrib.vertices[3 * idx.vertex_index + 1];
        auto vz = attrib.vertices[3 * idx.vertex_index + 2];
        glm::vec3 position(vx, vy, vz);
        glm::vec3 normal(0.0f, 1.0f, 0.0f);
        if (!attrib.normals.empty() && idx.normal_index >= 0) {
          auto nx = attrib.normals[3 * idx.normal_index + 0];
          auto ny = attrib.normals[3 * idx.normal_index + 1];
          auto nz = attrib.normals[3 * idx.normal_index + 2];
          normal = glm::vec3(nx, ny, nz);
        }
        SetTriangleVertex(tri, v, position, normal);
      }
      tris.push_back(tri);
      idx_offset += fv;
    }
  }
  return tris;
}

} // namespace graphics
