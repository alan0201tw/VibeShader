#version 460

// Fullscreen quad via vertex index — no vertex buffer.
// Useful as a base for fullscreen post-processing / ray marching shaders.

layout(location = 0) out vec2 frag_uv;

void main() {
  // Generate a fullscreen triangle (3 verts cover the screen):
  //   vertex 0: (-1, -1)  uv (0, 0)
  //   vertex 1: ( 3, -1)  uv (2, 0)
  //   vertex 2: (-1,  3)  uv (0, 2)
  frag_uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
  gl_Position = vec4(frag_uv * 2.0 - 1.0, 0.0, 1.0);
}
