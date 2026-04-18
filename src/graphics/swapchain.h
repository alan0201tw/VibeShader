#pragma once

#include <expected>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace graphics {

class VulkanContext;

// Owns the Vulkan swapchain and its image views.
class Swapchain {
 public:
  [[nodiscard]] static auto Create(const VulkanContext& context,
                                   GLFWwindow* window)
      -> std::expected<Swapchain, std::string>;

  ~Swapchain();

  Swapchain(const Swapchain&) = delete;
  Swapchain& operator=(const Swapchain&) = delete;
  Swapchain(Swapchain&& other) noexcept;
  Swapchain& operator=(Swapchain&& other) noexcept;

  [[nodiscard]] auto Handle() const -> VkSwapchainKHR { return swapchain_; }
  [[nodiscard]] auto Format() const -> VkFormat { return format_; }
  [[nodiscard]] auto Extent() const -> VkExtent2D { return extent_; }
  [[nodiscard]] auto ImageViews() const
      -> const std::vector<VkImageView>& { return image_views_; }

 private:
  Swapchain() = default;

  VkDevice device_ = VK_NULL_HANDLE;  // non-owning reference
  VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
  VkFormat format_ = VK_FORMAT_UNDEFINED;
  VkExtent2D extent_ = {0, 0};
  std::vector<VkImage> images_;
  std::vector<VkImageView> image_views_;
};

}  // namespace graphics
