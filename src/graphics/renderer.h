#pragma once

#include <expected>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

namespace graphics {

class VulkanContext;
class Swapchain;
class Pipeline;

// Simple renderer — owns command pool and buffers, handles presentation.
class Renderer {
 public:
  [[nodiscard]] static auto Create(const VulkanContext& context,
                                   const Swapchain& swapchain)
      -> std::expected<Renderer, std::string>;

  ~Renderer();

  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;
  Renderer(Renderer&& other) noexcept;
  Renderer& operator=(Renderer&& other) noexcept;

  // Record render pass and submit. Returns swapchain image index if successful.
  [[nodiscard]] auto BeginFrame() -> std::expected<uint32_t, std::string>;

  void RecordRenderPass(const Pipeline& pipeline, uint32_t image_index,
                        float time, float aspect_ratio);

  [[nodiscard]] auto EndFrameAndPresent(uint32_t image_index)
      -> std::expected<void, std::string>;

 private:
  Renderer() = default;

  VkDevice device_ = VK_NULL_HANDLE;  // non-owning
  VkQueue graphics_queue_ = VK_NULL_HANDLE;
  VkQueue present_queue_ = VK_NULL_HANDLE;
  VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;  // non-owning
  VkExtent2D swapchain_extent_ = {0, 0};
  VkCommandPool command_pool_ = VK_NULL_HANDLE;
  std::vector<VkCommandBuffer> command_buffers_;
  std::vector<VkSemaphore> image_available_semaphores_;
  std::vector<VkSemaphore> render_finished_semaphores_;
  std::vector<VkFence> in_flight_fences_;
  uint32_t current_frame_index_ = 0;  // Tracked frame for in-flight limiting
};

}  // namespace graphics
