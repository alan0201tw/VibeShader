#include "graphics/swapchain.h"

#include <algorithm>
#include <limits>
#include <vector>

#include <GLFW/glfw3.h>

#include "graphics/vulkan_context.h"

namespace graphics {
namespace {

auto ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& formats)
    -> VkSurfaceFormatKHR {
  for (const auto& fmt : formats) {
    if (fmt.format == VK_FORMAT_B8G8R8A8_SRGB &&
        fmt.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return fmt;
    }
  }
  return formats.front();
}

auto ChoosePresentMode(const std::vector<VkPresentModeKHR>& modes)
    -> VkPresentModeKHR {
  for (const auto& mode : modes) {
    if (mode == VK_PRESENT_MODE_MAILBOX_KHR) return mode;
  }
  return VK_PRESENT_MODE_FIFO_KHR;
}

auto ChooseExtent(const VkSurfaceCapabilitiesKHR& caps, GLFWwindow* window)
    -> VkExtent2D {
  if (caps.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
    return caps.currentExtent;
  }
  int width = 0;
  int height = 0;
  glfwGetFramebufferSize(window, &width, &height);
  VkExtent2D extent{static_cast<uint32_t>(width),
                    static_cast<uint32_t>(height)};
  extent.width = std::clamp(extent.width, caps.minImageExtent.width,
                            caps.maxImageExtent.width);
  extent.height = std::clamp(extent.height, caps.minImageExtent.height,
                             caps.maxImageExtent.height);
  return extent;
}

}  // namespace

auto Swapchain::Create(const VulkanContext& context, GLFWwindow* window)
    -> std::expected<Swapchain, std::string> {
  Swapchain sc;
  sc.device_ = context.Device();

  auto physical = context.PhysicalDevice();
  auto surface = context.Surface();

  // Query surface capabilities
  VkSurfaceCapabilitiesKHR caps{};
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &caps);

  uint32_t format_count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &format_count,
                                       nullptr);
  std::vector<VkSurfaceFormatKHR> formats(format_count);
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &format_count,
                                       formats.data());

  uint32_t mode_count = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &mode_count,
                                            nullptr);
  std::vector<VkPresentModeKHR> modes(mode_count);
  vkGetPhysicalDeviceSurfacePresentModesKHR(physical, surface, &mode_count,
                                            modes.data());

  if (formats.empty() || modes.empty()) {
    return std::unexpected("Swapchain not supported on this surface");
  }

  auto surface_format = ChooseSurfaceFormat(formats);
  auto present_mode = ChoosePresentMode(modes);
  auto extent = ChooseExtent(caps, window);

  uint32_t image_count = caps.minImageCount + 1;
  if (caps.maxImageCount > 0 && image_count > caps.maxImageCount) {
    image_count = caps.maxImageCount;
  }

  VkSwapchainCreateInfoKHR info{};
  info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  info.surface = surface;
  info.minImageCount = image_count;
  info.imageFormat = surface_format.format;
  info.imageColorSpace = surface_format.colorSpace;
  info.imageExtent = extent;
  info.imageArrayLayers = 1;
  info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  uint32_t queue_families[] = {context.GraphicsQueueFamily(),
                               context.PresentQueueFamily()};
  if (queue_families[0] != queue_families[1]) {
    info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    info.queueFamilyIndexCount = 2;
    info.pQueueFamilyIndices = queue_families;
  } else {
    info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  info.preTransform = caps.currentTransform;
  info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  info.presentMode = present_mode;
  info.clipped = VK_TRUE;
  info.oldSwapchain = VK_NULL_HANDLE;

  if (vkCreateSwapchainKHR(sc.device_, &info, nullptr, &sc.swapchain_) !=
      VK_SUCCESS) {
    return std::unexpected("Failed to create swapchain");
  }

  sc.format_ = surface_format.format;
  sc.extent_ = extent;

  // Retrieve images
  vkGetSwapchainImagesKHR(sc.device_, sc.swapchain_, &image_count, nullptr);
  sc.images_.resize(image_count);
  vkGetSwapchainImagesKHR(sc.device_, sc.swapchain_, &image_count,
                          sc.images_.data());

  // Create image views
  sc.image_views_.resize(image_count);
  for (uint32_t i = 0; i < image_count; ++i) {
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = sc.images_[i];
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = sc.format_;
    view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    if (vkCreateImageView(sc.device_, &view_info, nullptr,
                          &sc.image_views_[i]) != VK_SUCCESS) {
      return std::unexpected("Failed to create image view");
    }
  }

  return sc;
}

Swapchain::~Swapchain() {
  if (device_ == VK_NULL_HANDLE) return;
  for (auto view : image_views_) {
    vkDestroyImageView(device_, view, nullptr);
  }
  if (swapchain_ != VK_NULL_HANDLE) {
    vkDestroySwapchainKHR(device_, swapchain_, nullptr);
  }
}

Swapchain::Swapchain(Swapchain&& other) noexcept
    : device_(other.device_),
      swapchain_(other.swapchain_),
      format_(other.format_),
      extent_(other.extent_),
      images_(std::move(other.images_)),
      image_views_(std::move(other.image_views_)) {
  other.device_ = VK_NULL_HANDLE;
  other.swapchain_ = VK_NULL_HANDLE;
}

Swapchain& Swapchain::operator=(Swapchain&& other) noexcept {
  if (this != &other) {
    for (auto view : image_views_) {
      vkDestroyImageView(device_, view, nullptr);
    }
    if (swapchain_ != VK_NULL_HANDLE) {
      vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    }

    device_ = other.device_;
    swapchain_ = other.swapchain_;
    format_ = other.format_;
    extent_ = other.extent_;
    images_ = std::move(other.images_);
    image_views_ = std::move(other.image_views_);

    other.device_ = VK_NULL_HANDLE;
    other.swapchain_ = VK_NULL_HANDLE;
  }
  return *this;
}

}  // namespace graphics
