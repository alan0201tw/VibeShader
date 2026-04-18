#include <gtest/gtest.h>

#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>

namespace {

TEST(MathSanity, Vec3DotProduct) {
  glm::vec3 a(1.0f, 0.0f, 0.0f);
  glm::vec3 b(0.0f, 1.0f, 0.0f);
  EXPECT_FLOAT_EQ(glm::dot(a, b), 0.0f);
}

TEST(MathSanity, Vec3CrossProduct) {
  glm::vec3 x(1.0f, 0.0f, 0.0f);
  glm::vec3 y(0.0f, 1.0f, 0.0f);
  glm::vec3 z = glm::cross(x, y);
  // Right-handed: X × Y = Z
  EXPECT_TRUE(glm::all(glm::epsilonEqual(z, glm::vec3(0, 0, 1), 1e-6f)));
}

TEST(MathSanity, Vec3Normalize) {
  glm::vec3 v(3.0f, 4.0f, 0.0f);
  glm::vec3 n = glm::normalize(v);
  EXPECT_NEAR(glm::length(n), 1.0f, 1e-6f);
}

}  // namespace
