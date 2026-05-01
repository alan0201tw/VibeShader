#pragma once

#include <vulkan/vulkan.h>
#include <cstddef>
#include <span>
#include <expected>
#include <string>

namespace graphics {

// Creates a device-local buffer and uploads data to it (host-visible staging).
// Returns buffer and device memory. Caller must destroy both.
std::expected<std::pair<VkBuffer, VkDeviceMemory>, std::string>
CreateDeviceBuffer(VkPhysicalDevice physical_device, VkDevice device,
                   std::span<const std::byte> data,
                   VkBufferUsageFlags usage);

} // namespace graphics
