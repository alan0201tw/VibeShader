#include "graphics/buffer_util.h"
#include <cstring>
#include <cstddef>

namespace graphics {

namespace {
auto FindMemoryType(VkPhysicalDevice physical_device, uint32_t type_filter,
                    VkMemoryPropertyFlags properties) -> uint32_t {
  VkPhysicalDeviceMemoryProperties mem_props;
  vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_props);
  for (auto i = uint32_t{0}; i < mem_props.memoryTypeCount; i++) {
    if ((type_filter & (1 << i)) &&
        (mem_props.memoryTypes[i].propertyFlags & properties) ==
            properties) {
      return i;
    }
  }
  return UINT32_MAX;
}
}  // namespace

std::expected<std::pair<VkBuffer, VkDeviceMemory>, std::string>
CreateDeviceBuffer(VkPhysicalDevice physical_device, VkDevice device,
                   std::span<const std::byte> data,
                   VkBufferUsageFlags usage) {
  VkBuffer staging_buffer;
  VkDeviceMemory staging_memory;
  VkBufferCreateInfo buf_info{};
  buf_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buf_info.size = data.size_bytes();
  buf_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
  buf_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  if (vkCreateBuffer(device, &buf_info, nullptr, &staging_buffer) !=
      VK_SUCCESS) {
    return std::unexpected("Failed to create staging buffer");
  }
  VkMemoryRequirements mem_req;
  vkGetBufferMemoryRequirements(device, staging_buffer, &mem_req);
  VkMemoryAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.allocationSize = mem_req.size;
  alloc_info.memoryTypeIndex = FindMemoryType(
      physical_device, mem_req.memoryTypeBits,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  if (alloc_info.memoryTypeIndex == UINT32_MAX) {
    return std::unexpected("No suitable memory type for staging buffer");
  }
  if (vkAllocateMemory(device, &alloc_info, nullptr, &staging_memory) !=
      VK_SUCCESS) {
    return std::unexpected("Failed to allocate staging buffer memory");
  }
  vkBindBufferMemory(device, staging_buffer, staging_memory, 0);
  void* mapped;
  vkMapMemory(device, staging_memory, 0, data.size_bytes(), 0, &mapped);
  std::memcpy(mapped, data.data(), data.size_bytes());
  vkUnmapMemory(device, staging_memory);

  VkBuffer device_buffer;
  VkDeviceMemory device_memory;
  buf_info.usage = usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  if (vkCreateBuffer(device, &buf_info, nullptr, &device_buffer) !=
      VK_SUCCESS) {
    return std::unexpected("Failed to create device buffer");
  }
  vkGetBufferMemoryRequirements(device, device_buffer, &mem_req);
  alloc_info.allocationSize = mem_req.size;
  alloc_info.memoryTypeIndex = FindMemoryType(
      physical_device, mem_req.memoryTypeBits,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  if (alloc_info.memoryTypeIndex == UINT32_MAX) {
    return std::unexpected("No suitable memory type for device buffer");
  }
  if (vkAllocateMemory(device, &alloc_info, nullptr, &device_memory) !=
      VK_SUCCESS) {
    return std::unexpected("Failed to allocate device buffer memory");
  }
  vkBindBufferMemory(device, device_buffer, device_memory, 0);

  // Copy staging to device
  VkCommandPool pool;
  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.queueFamilyIndex = 0; // Assume GFX queue family 0 (fix if needed)
  pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
  vkCreateCommandPool(device, &pool_info, nullptr, &pool);
  VkCommandBuffer cmd;
  VkCommandBufferAllocateInfo cmd_info{};
  cmd_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cmd_info.commandPool = pool;
  cmd_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cmd_info.commandBufferCount = 1;
  vkAllocateCommandBuffers(device, &cmd_info, &cmd);
  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cmd, &begin_info);
  VkBufferCopy copy{};
  copy.size = data.size_bytes();
  vkCmdCopyBuffer(cmd, staging_buffer, device_buffer, 1, &copy);
  vkEndCommandBuffer(cmd);
  VkSubmitInfo submit{};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit.commandBufferCount = 1;
  submit.pCommandBuffers = &cmd;
  VkQueue queue;
  vkGetDeviceQueue(device, 0, 0, &queue); // Assume queue family 0
  vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
  vkQueueWaitIdle(queue);
  vkFreeCommandBuffers(device, pool, 1, &cmd);
  vkDestroyCommandPool(device, pool, nullptr);
  vkDestroyBuffer(device, staging_buffer, nullptr);
  vkFreeMemory(device, staging_memory, nullptr);
  return std::make_pair(device_buffer, device_memory);
}

}  // namespace graphics
