#include <gtest/gtest.h>

#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>

namespace {

TEST(MathSanity, Vec3DotProduct) {
  auto a = glm::vec3(1.0f, 0.0f, 0.0f);
  auto b = glm::vec3(0.0f, 1.0f, 0.0f);
  EXPECT_FLOAT_EQ(glm::dot(a, b), 0.0f);
}

TEST(MathSanity, Vec3CrossProduct) {
  auto x = glm::vec3(1.0f, 0.0f, 0.0f);
  auto y = glm::vec3(0.0f, 1.0f, 0.0f);
  auto z = glm::cross(x, y);
  // Right-handed: X × Y = Z
  EXPECT_TRUE(glm::all(glm::epsilonEqual(z, glm::vec3(0, 0, 1), 1e-6f)));
}

TEST(MathSanity, Vec3Normalize) {
  auto v = glm::vec3(3.0f, 4.0f, 0.0f);
  auto n = glm::normalize(v);
  EXPECT_NEAR(glm::length(n), 1.0f, 1e-6f);
}

}  // namespace
