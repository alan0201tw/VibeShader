#include "graphics/vulkan_context.h"
#include "graphics/pipeline.h"
#include "graphics/renderer.h"
#include "graphics/swapchain.h"

#include <chrono>
#include <cstdlib>
#include <string_view>
#include <print>

#include <GLFW/glfw3.h>

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr const char* kWindowTitle = "Playground — Vulkan + GLSL";

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

  auto pipeline_result = graphics::Pipeline::Create(
      context, swapchain, SHADER_DIR "/fullscreen.vert.spv",
      SHADER_DIR "/fullscreen.frag.spv");
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
        SHADER_DIR "/fullscreen.frag.spv");
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

    renderer.RecordRenderPass(pipeline, image_index, elapsed_seconds,
                              aspect_ratio);

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

  glfwDestroyWindow(window);
  glfwTerminate();
  return EXIT_SUCCESS;
}
