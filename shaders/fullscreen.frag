#version 460

// Simple time-based animation shader — rotating rainbow gradient with pulsing waves.
// Demonstrates push constants for time and aspect ratio.

layout(location = 0) in vec2 frag_uv;

layout(location = 0) out vec4 out_color;

layout(push_constant) uniform PushConstants {
  float time;      // elapsed time in seconds
  float aspect;    // width / height
} pc;

// Simple HSV to RGB conversion
vec3 HsvToRgb(float hue, float saturation, float value) {
  float h = hue * 6.0;
  int i = int(h);
  float f = h - float(i);
  float p = value * (1.0 - saturation);
  float q = value * (1.0 - saturation * f);
  float t = value * (1.0 - saturation * (1.0 - f));

  if (i == 0) return vec3(value, t, p);
  else if (i == 1) return vec3(q, value, p);
  else if (i == 2) return vec3(p, value, t);
  else if (i == 3) return vec3(p, q, value);
  else if (i == 4) return vec3(t, p, value);
  else return vec3(value, p, q);
}

void main() {
  // Normalized screen coordinates centered at origin
  vec2 uv = frag_uv * 2.0 - 1.0;
  uv.x *= pc.aspect;

  // Polar coordinates (angle and distance from center)
  float angle = atan(uv.y, uv.x);
  float radius = length(uv);

  // Rotating hue: angle rotates with time, creating a spinning rainbow
  float hue = fract((angle / 6.28318) + pc.time * 0.5);

  // Pulsing radial waves: distance-based ripple effect driven by time
  float wave = sin(radius * 4.0 - pc.time * 3.0) * 0.5 + 0.5;

  // Overall brightness pulsing up and down
  float brightness = 0.6 + 0.3 * sin(pc.time);

  // Combine effects: apply saturation fade at edges
  float saturation = mix(1.0, 0.3, smoothstep(0.0, 2.0, radius));

  // Convert HSV to RGB and apply wave modulation
  vec3 color = HsvToRgb(hue, saturation, brightness * wave);

  // Fade to black at edges
  color *= max(0.0, 1.0 - radius * 0.5);

  out_color = vec4(color, 1.0);
}
