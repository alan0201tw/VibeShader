#pragma once

#include <expected>
#include <string>

#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace graphics {

// Owns the Vulkan instance, physical device, logical device, and queues.
// RAII: resources are destroyed in the destructor.
class VulkanContext {
 public:
  [[nodiscard]] static auto Create(GLFWwindow* window)
      -> std::expected<VulkanContext, std::string>;

  ~VulkanContext();

  VulkanContext(const VulkanContext&) = delete;
  VulkanContext& operator=(const VulkanContext&) = delete;
  VulkanContext(VulkanContext&& other) noexcept;
  VulkanContext& operator=(VulkanContext&& other) noexcept;

  [[nodiscard]] auto Instance() const -> VkInstance { return instance_; }
  [[nodiscard]] auto Surface() const -> VkSurfaceKHR { return surface_; }
  [[nodiscard]] auto PhysicalDevice() const -> VkPhysicalDevice {
    return physical_device_;
  }
  [[nodiscard]] auto Device() const -> VkDevice { return device_; }
  [[nodiscard]] auto GraphicsQueue() const -> VkQueue {
    return graphics_queue_;
  }
  [[nodiscard]] auto PresentQueue() const -> VkQueue {
    return present_queue_;
  }
  [[nodiscard]] auto GraphicsQueueFamily() const -> uint32_t {
    return graphics_queue_family_;
  }
  [[nodiscard]] auto PresentQueueFamily() const -> uint32_t {
    return present_queue_family_;
  }

  void WaitIdle() const;

 private:
  VulkanContext() = default;

  VkInstance instance_ = VK_NULL_HANDLE;
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;
  VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue graphics_queue_ = VK_NULL_HANDLE;
  VkQueue present_queue_ = VK_NULL_HANDLE;
  uint32_t graphics_queue_family_ = 0;
  uint32_t present_queue_family_ = 0;
  VkDebugUtilsMessengerEXT debug_messenger_ = VK_NULL_HANDLE;
};

}  // namespace graphics
