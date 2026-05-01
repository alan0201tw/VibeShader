#version 460

// Real-time analytic ray tracer with a PBR material model.
//
// Highlights
//   - Metallic/roughness workflow materials
//   - GGX microfacet BRDF + Schlick Fresnel + Smith masking
//   - Emissive sphere lights with shadow rays
//   - Multi-bounce reflections for glossy and mirror materials
//   - Filmic tonemapping and gamma correction

layout(location = 0) in vec2 frag_uv;
layout(location = 0) out vec4 out_color;


layout(push_constant) uniform PushConstants {
  float time;
  float aspect;
  int triangle_count;
  int _pad;
} pc;

struct MeshTriangle {
  vec3 v0;
  float pad0;
  vec3 v1;
  float pad1;
  vec3 v2;
  float pad2;
  vec3 n0;
  float pad3;
  vec3 n1;
  float pad4;
  vec3 n2;
  float pad5;
};

layout(set = 0, binding = 0) readonly buffer MeshBuffer {
  MeshTriangle tris[];
};



const float kPi = 3.14159265359;
const int kMaxBounces = 4;
const int kSphereCount = 9;

struct Ray {
  vec3 origin;
  vec3 dir;
};

struct Material {
  vec3 albedo;
  float metallic;
  float roughness;
  float emission;
};

struct Sphere {
  vec3 center;
  float radius;
  Material mat;
};


struct HitRecord {
  float t;
  vec3 pos;
  vec3 normal;
  int sphere_idx;
  int tri_idx;
};

Sphere g_spheres[kSphereCount];

float Saturate(float x) {
  return clamp(x, 0.0, 1.0);
}

vec3 SkyColor(vec3 dir) {
  float h = Saturate(0.5 * (dir.y + 1.0));
  vec3 horizon = vec3(0.92, 0.90, 0.86);
  vec3 zenith = vec3(0.32, 0.52, 0.88);
  vec3 sky = mix(horizon, zenith, h);

  vec3 sun_dir = normalize(vec3(0.5, 0.75, -0.4));
  float sun_amount = pow(Saturate(dot(dir, sun_dir)), 900.0);
  sky += vec3(8.5, 7.8, 6.6) * sun_amount;

  return sky;
}

float DistributionGGX(vec3 n, vec3 h, float roughness) {
  float a = roughness * roughness;
  float a2 = a * a;
  float ndoth = Saturate(dot(n, h));
  float ndoth2 = ndoth * ndoth;
  float denom = ndoth2 * (a2 - 1.0) + 1.0;
  return a2 / max(kPi * denom * denom, 1e-5);
}

float GeometrySchlickGGX(float ndotv, float roughness) {
  float r = roughness + 1.0;
  float k = (r * r) * 0.125;
  return ndotv / max(ndotv * (1.0 - k) + k, 1e-5);
}

float GeometrySmith(vec3 n, vec3 v, vec3 l, float roughness) {
  float ndotv = Saturate(dot(n, v));
  float ndotl = Saturate(dot(n, l));
  float ggx_v = GeometrySchlickGGX(ndotv, roughness);
  float ggx_l = GeometrySchlickGGX(ndotl, roughness);
  return ggx_v * ggx_l;
}

vec3 FresnelSchlick(float cos_theta, vec3 f0) {
  return f0 + (1.0 - f0) * pow(1.0 - cos_theta, 5.0);
}

void BuildScene() {
  float t = pc.time;

  g_spheres[0] = Sphere(
      vec3(0.0, -100.55, -3.0), 100.0,
      Material(vec3(0.62, 0.62, 0.64), 0.0, 0.55, 0.0));

  g_spheres[1] = Sphere(
      vec3(-1.35, 0.0, -3.4), 0.62,
      Material(vec3(0.97, 0.97, 0.98), 1.0, 0.08, 0.0));

  g_spheres[2] = Sphere(
      vec3(0.0, 0.0, -2.9), 0.62,
      Material(vec3(0.78, 0.09, 0.08), 0.0, 0.22, 0.0));

  g_spheres[3] = Sphere(
      vec3(1.35, 0.0, -3.2), 0.62,
      Material(vec3(1.00, 0.77, 0.33), 1.0, 0.18, 0.0));

  float a0 = t * 0.7;
  g_spheres[4] = Sphere(
      vec3(sin(a0) * 2.1, 0.45 + 0.2 * sin(a0 * 1.7), -3.0 + cos(a0) * 1.4),
      0.24,
      Material(vec3(0.18, 0.52, 1.0), 0.0, 0.12, 0.0));

  float a1 = t * 1.2 + 2.2;
  g_spheres[5] = Sphere(
      vec3(sin(a1) * 1.6, 0.25, -3.0 + cos(a1) * 2.0),
      0.2,
      Material(vec3(0.92, 0.45, 0.12), 0.35, 0.3, 0.0));

  g_spheres[6] = Sphere(
      vec3(2.6, 3.6, -2.2), 0.46,
      Material(vec3(1.0, 0.93, 0.78), 0.0, 0.05, 14.0));

  float pulse = 0.12 + 0.08 * sin(t * 1.8);
  g_spheres[7] = Sphere(
      vec3(-2.8, 2.4, -4.1), 0.2 + pulse,
      Material(vec3(0.55, 0.82, 1.0), 0.0, 0.06, 10.0));

  g_spheres[8] = Sphere(
      vec3(0.0, 4.7, -5.0), 0.75,
      Material(vec3(1.0, 0.55, 0.35), 0.0, 0.06, 7.5));
}

bool IntersectSphere(Ray ray, Sphere s, out float t_hit) {
  vec3 oc = ray.origin - s.center;
  float a = dot(ray.dir, ray.dir);
  float half_b = dot(oc, ray.dir);
  float c = dot(oc, oc) - s.radius * s.radius;
  float disc = half_b * half_b - a * c;
  if (disc < 0.0) {
    return false;
  }

  float sq = sqrt(disc);
  float root = (-half_b - sq) / a;
  if (root < 1e-4) {
    root = (-half_b + sq) / a;
    if (root < 1e-4) {
      return false;
    }
  }
  t_hit = root;
  return true;
}


bool IntersectTriangle(Ray ray, MeshTriangle tri, out float t, out vec3 normal) {
  vec3 v0v1 = tri.v1 - tri.v0;
  vec3 v0v2 = tri.v2 - tri.v0;
  vec3 pvec = cross(ray.dir, v0v2);
  float det = dot(v0v1, pvec);
  if (abs(det) < 1e-8) return false;
  float invDet = 1.0 / det;
  vec3 tvec = ray.origin - tri.v0;
  float u = dot(tvec, pvec) * invDet;
  if (u < 0.0 || u > 1.0) return false;
  vec3 qvec = cross(tvec, v0v1);
  float v = dot(ray.dir, qvec) * invDet;
  if (v < 0.0 || u + v > 1.0) return false;
  float tt = dot(v0v2, qvec) * invDet;
  if (tt < 1e-4) return false;
  t = tt;
  normal = normalize(cross(v0v1, v0v2));
  return true;
}

HitRecord TraceScene(Ray ray) {
  HitRecord rec;
  rec.t = 1e30;
  rec.sphere_idx = -1;
  rec.tri_idx = -1;

  // Spheres
  for (int i = 0; i < kSphereCount; ++i) {
    float t_hit;
    if (IntersectSphere(ray, g_spheres[i], t_hit) && t_hit < rec.t) {
      rec.t = t_hit;
      rec.sphere_idx = i;
      rec.tri_idx = -1;
      rec.pos = ray.origin + ray.dir * t_hit;
      rec.normal = normalize(rec.pos - g_spheres[i].center);
    }
  }
  // Triangles
  for (int i = 0; i < pc.triangle_count; ++i) {
    float t_hit;
    vec3 n_hit;
    if (IntersectTriangle(ray, tris[i], t_hit, n_hit) && t_hit < rec.t) {
      rec.t = t_hit;
      rec.tri_idx = i;
      rec.sphere_idx = -1;
      rec.pos = ray.origin + ray.dir * t_hit;
      rec.normal = n_hit;
    }
  }
  return rec;
}

vec3 EvaluateDirectLighting(Material mat, vec3 pos, vec3 n, vec3 v) {
  vec3 f0 = mix(vec3(0.04), mat.albedo, mat.metallic);
  vec3 direct = vec3(0.0);

  for (int li = 0; li < kSphereCount; ++li) {
    if (g_spheres[li].mat.emission <= 0.0) {
      continue;
    }

    vec3 to_light = g_spheres[li].center - pos;
    float dist = length(to_light);
    vec3 l = to_light / dist;
    vec3 h = normalize(v + l);

    Ray shadow;
    shadow.origin = pos + n * 1e-3;
    shadow.dir = l;
    HitRecord sh = TraceScene(shadow);
    if (sh.sphere_idx >= 0 && sh.t < dist - g_spheres[li].radius) {
      continue;
    }

    float ndotl = Saturate(dot(n, l));
    float ndotv = Saturate(dot(n, v));
    float hdotv = Saturate(dot(h, v));
    if (ndotl <= 0.0 || ndotv <= 0.0) {
      continue;
    }

    float d = DistributionGGX(n, h, mat.roughness);
    float g = GeometrySmith(n, v, l, mat.roughness);
    vec3 f = FresnelSchlick(hdotv, f0);

    vec3 spec = (d * g * f) / max(4.0 * ndotv * ndotl, 1e-5);
    vec3 kd = (1.0 - f) * (1.0 - mat.metallic);
    vec3 diff = kd * mat.albedo / kPi;

    float radius = g_spheres[li].radius;
    float area_scale = radius * radius * 4.0;
    float attenuation = area_scale / (1.0 + dist * dist);
    vec3 radiance = g_spheres[li].mat.albedo * g_spheres[li].mat.emission * attenuation;

    direct += (diff + spec) * radiance * ndotl;
  }

  return direct;
}

vec3 Shade(Ray primary_ray) {
  vec3 result = vec3(0.0);
  vec3 throughput = vec3(1.0);
  Ray ray = primary_ray;

  for (int bounce = 0; bounce < kMaxBounces; ++bounce) {
    HitRecord rec = TraceScene(ray);
    if (rec.sphere_idx < 0 && rec.tri_idx < 0) {
      result += throughput * SkyColor(normalize(ray.dir));
      break;
    }

    Material mat;
    if (rec.sphere_idx >= 0) {
      mat = g_spheres[rec.sphere_idx].mat;
    } else if (rec.tri_idx >= 0) {
      mat.albedo = vec3(0.7, 0.7, 0.7); // Flat gray for mesh
      mat.metallic = 0.0;
      mat.roughness = 0.3;
      mat.emission = 0.0;
    }

    if (mat.emission > 0.0) {
      result += throughput * mat.albedo * mat.emission;
      break;
    }

    vec3 n = rec.normal;
    vec3 v = normalize(-ray.dir);
    vec3 f0 = mix(vec3(0.04), mat.albedo, mat.metallic);

    vec3 direct = EvaluateDirectLighting(mat, rec.pos, n, v);

    vec3 env_ambient = SkyColor(n) * 0.12;
    vec3 f_ambient = FresnelSchlick(Saturate(dot(n, v)), f0);
    vec3 kd_ambient = (1.0 - f_ambient) * (1.0 - mat.metallic);
    vec3 ambient = kd_ambient * mat.albedo * env_ambient;

    result += throughput * (direct + ambient);

    vec3 r = reflect(ray.dir, n);
    vec3 rough_reflect = normalize(mix(r, n, mat.roughness * mat.roughness));

    vec3 f = FresnelSchlick(Saturate(dot(n, v)), f0);
    vec3 spec_weight = mix(f, vec3(1.0), mat.metallic * 0.5);
    throughput *= spec_weight;

    if (max(throughput.r, max(throughput.g, throughput.b)) < 0.01) {
      break;
    }

    ray.origin = rec.pos + n * 1e-3;
    ray.dir = rough_reflect;
  }

  return result;
}

void main() {
  BuildScene();

  vec2 uv = frag_uv * 2.0 - 1.0;
  uv.x *= pc.aspect;

  const float kFov = 0.72;
  vec3 cam_pos = vec3(0.0, 0.55, 5.4);
  vec3 cam_target = vec3(0.0, 0.15, 0.0);
  vec3 cam_fwd = normalize(cam_target - cam_pos);
  vec3 cam_right = normalize(cross(cam_fwd, vec3(0.0, -1.0, 0.0)));
  vec3 cam_up = cross(cam_right, cam_fwd);

  Ray ray;
  ray.origin = cam_pos;
  ray.dir = normalize(cam_fwd + uv.x * kFov * cam_right + uv.y * kFov * cam_up);

  vec3 color = Shade(ray);

  color = color / (color + vec3(1.0));
  color = pow(max(color, vec3(0.0)), vec3(1.0 / 2.2));

  out_color = vec4(color, 1.0);
}
