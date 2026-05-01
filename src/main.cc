#include "graphics/vulkan_context.h"
#include "graphics/pipeline.h"
#include "graphics/renderer.h"
#include "graphics/swapchain.h"
#include "graphics/mesh_loader.h"
#include "graphics/buffer_util.h"
#include <span>

#include <chrono>
#include <cstdlib>
#include <string_view>
#include <print>

#include <GLFW/glfw3.h>

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr const char* kWindowTitle = "Playground — Real-Time Ray Tracer";

}  // namespace

int main() {
  if (!glfwInit()) {
    std::println(stderr, "Failed to initialize GLFW");
    return EXIT_FAILURE;
  }

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);  // Vulkan — no OpenGL context
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

  GLFWwindow* window = glfwCreateWindow(
      kWindowWidth, kWindowHeight, kWindowTitle, nullptr, nullptr);
  if (!window) {
    std::println(stderr, "Failed to create GLFW window");
    glfwTerminate();
    return EXIT_FAILURE;
  }

  // ── Vulkan initialization ───────────────────────────────────────────────
  auto context_result = graphics::VulkanContext::Create(window);
  if (!context_result) {
    std::println(stderr, "Vulkan init failed: {}", context_result.error());
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_FAILURE;
  }
  auto& context = *context_result;

  auto swapchain_result = graphics::Swapchain::Create(context, window);
  if (!swapchain_result) {
    std::println(stderr, "Swapchain creation failed: {}",
                 swapchain_result.error());
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_FAILURE;
  }
  auto& swapchain = *swapchain_result;

  // ── Load mesh and upload to GPU ───────────────────────────────────────

  std::vector<graphics::MeshTriangle> mesh_tris = graphics::LoadMeshFromObj("assets/teapot.obj");
  if (mesh_tris.empty()) {
    std::println(stderr, "Failed to load mesh or mesh is empty");
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_FAILURE;
  }

  // --- Normalize and center mesh ---
  glm::vec3 min_v(1e30f), max_v(-1e30f);
  for (const auto& tri : mesh_tris) {
    for (const glm::vec3& v : {tri.v0, tri.v1, tri.v2}) {
      min_v = glm::min(min_v, v);
      max_v = glm::max(max_v, v);
    }
  }
  glm::vec3 center = 0.5f * (min_v + max_v);
  glm::vec3 extent = max_v - min_v;
  float max_extent = std::max({extent.x, extent.y, extent.z});
  float scale = 2.0f / max_extent; // fit in [-1,1]
  for (auto& tri : mesh_tris) {
    tri.v0 = (tri.v0 - center) * scale;
    tri.v1 = (tri.v1 - center) * scale;
    tri.v2 = (tri.v2 - center) * scale;
  }

  auto mesh_buffer_result = graphics::CreateDeviceBuffer(
      context.PhysicalDevice(), context.Device(),
      std::as_bytes(std::span(mesh_tris)),
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
  if (!mesh_buffer_result) {
    std::println(stderr, "Failed to create mesh buffer: {}", mesh_buffer_result.error());
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_FAILURE;
  }
  VkBuffer mesh_buffer = mesh_buffer_result->first;
  VkDeviceMemory mesh_memory = mesh_buffer_result->second;

  auto pipeline_result = graphics::Pipeline::Create(
      context, swapchain, SHADER_DIR "/fullscreen.vert.spv",
      SHADER_DIR "/raytracer.frag.spv");
  if (!pipeline_result) {
    std::println(stderr, "Pipeline creation failed: {}",
                 pipeline_result.error());
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_FAILURE;
  }
  auto& pipeline = *pipeline_result;

  // Renderer initialization
  auto renderer_result = graphics::Renderer::Create(context, swapchain);
  if (!renderer_result) {
    std::println(stderr, "Renderer creation failed: {}",
                 renderer_result.error());
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_FAILURE;
  }
  auto& renderer = *renderer_result;

  // ── Descriptor pool and set for mesh buffer ──────────────────────────
  VkDescriptorPoolSize pool_size{};
  pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  pool_size.descriptorCount = 1;

  VkDescriptorPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.maxSets = 1;
  pool_info.poolSizeCount = 1;
  pool_info.pPoolSizes = &pool_size;
  VkDescriptorPool descriptor_pool;
  if (vkCreateDescriptorPool(context.Device(), &pool_info, nullptr, &descriptor_pool) != VK_SUCCESS) {
    std::println(stderr, "Failed to create descriptor pool");
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_FAILURE;
  }

  VkDescriptorSetAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = descriptor_pool;
  alloc_info.descriptorSetCount = 1;
  const auto& layout = pipeline.DescriptorSetLayout();
  alloc_info.pSetLayouts = &layout;

  VkDescriptorSet mesh_descriptor_set;
  if (vkAllocateDescriptorSets(context.Device(), &alloc_info, &mesh_descriptor_set) != VK_SUCCESS) {
    std::println(stderr, "Failed to allocate descriptor set");
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_FAILURE;
  }

  VkDescriptorBufferInfo buffer_info{};
  buffer_info.buffer = mesh_buffer;
  buffer_info.offset = 0;
  buffer_info.range = mesh_tris.size() * sizeof(graphics::MeshTriangle);

  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = mesh_descriptor_set;
  write.dstBinding = 0;
  write.dstArrayElement = 0;
  write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  write.descriptorCount = 1;
  write.pBufferInfo = &buffer_info;
  vkUpdateDescriptorSets(context.Device(), 1, &write, 0, nullptr);

  auto recreate_swapchain_resources = [&]() -> bool {
    int framebuffer_width = 0;
    int framebuffer_height = 0;
    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
    while (framebuffer_width == 0 || framebuffer_height == 0) {
      glfwWaitEvents();
      glfwGetFramebufferSize(window, &framebuffer_width,
                             &framebuffer_height);
    }

    context.WaitIdle();

    auto new_swapchain_result = graphics::Swapchain::Create(context, window);
    if (!new_swapchain_result) {
      std::println(stderr, "Swapchain recreation failed: {}",
                   new_swapchain_result.error());
      return false;
    }

    auto new_pipeline_result = graphics::Pipeline::Create(
        context, *new_swapchain_result, SHADER_DIR "/fullscreen.vert.spv",
        SHADER_DIR "/raytracer.frag.spv");
    if (!new_pipeline_result) {
      std::println(stderr, "Pipeline recreation failed: {}",
                   new_pipeline_result.error());
      return false;
    }

    auto new_renderer_result =
        graphics::Renderer::Create(context, *new_swapchain_result);
    if (!new_renderer_result) {
      std::println(stderr, "Renderer recreation failed: {}",
                   new_renderer_result.error());
      return false;
    }

    swapchain = std::move(*new_swapchain_result);
    pipeline = std::move(*new_pipeline_result);
    renderer = std::move(*new_renderer_result);
    return true;
  };

  std::println("Playground running — press ESC or close window to exit");

  // Record current time for delta calculation
  auto start_time = std::chrono::high_resolution_clock::now();

  // ── Main loop ───────────────────────────────────────────────────────────
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
      glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    // Calculate elapsed time in seconds for shader animation
    auto current_time = std::chrono::high_resolution_clock::now();
    float elapsed_seconds =
        std::chrono::duration<float>(current_time - start_time).count();

    // Calculate aspect ratio for screen coordinates in shader
    float aspect_ratio =
      static_cast<float>(swapchain.Extent().width) /
      static_cast<float>(swapchain.Extent().height);

    // Frame rendering
    auto image_idx_result = renderer.BeginFrame();
    if (!image_idx_result) {
      if (image_idx_result.error() == std::string_view{"Swapchain out of date"}) {
        if (!recreate_swapchain_resources()) {
          break;
        }
        continue;
      }
      std::println(stderr, "Failed to begin frame: {}",
                   image_idx_result.error());
      continue;
    }
    uint32_t image_index = *image_idx_result;

    // Pass triangle count as push constant (time, aspect, triangle_count)
    struct PushConstants {
      float time;
      float aspect;
      int triangle_count;
      int _pad = 0;
    } pc;
    pc.time = elapsed_seconds;
    pc.aspect = aspect_ratio;
    pc.triangle_count = static_cast<int>(mesh_tris.size());
    pc._pad = 0;

    renderer.RecordRenderPass(pipeline, image_index, &pc, sizeof(pc), mesh_descriptor_set);

    auto present_result = renderer.EndFrameAndPresent(image_index);
    if (!present_result) {
      if (present_result.error() == std::string_view{"Swapchain out of date"}) {
        if (!recreate_swapchain_resources()) {
          break;
        }
        continue;
      }
      std::println(stderr, "Failed to present: {}", present_result.error());
      continue;
    }
  }

  // Wait for GPU to finish before cleanup
  context.WaitIdle();

  // Cleanup mesh buffer
  vkDestroyBuffer(context.Device(), mesh_buffer, nullptr);
  vkFreeMemory(context.Device(), mesh_memory, nullptr);

  glfwDestroyWindow(window);
  glfwTerminate();
  return EXIT_SUCCESS;
}
