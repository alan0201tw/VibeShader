#include "graphics/shader_module.h"

#include <fstream>
#include <print>

namespace graphics {

auto ShaderModule::CreateFromFile(VkDevice device,
                                  const std::string& spirv_path)
    -> std::expected<ShaderModule, std::string> {
  std::ifstream file(spirv_path, std::ios::ate | std::ios::binary);
  if (!file.is_open()) {
    return std::unexpected(
        std::format("Failed to open shader file: {}", spirv_path));
  }

  auto file_size = static_cast<size_t>(file.tellg());
  if (file_size % sizeof(uint32_t) != 0) {
    return std::unexpected("Shader file size is not aligned to 4 bytes");
  }

  std::vector<uint32_t> code(file_size / sizeof(uint32_t));
  file.seekg(0);
  file.read(reinterpret_cast<char*>(code.data()),
            static_cast<std::streamsize>(file_size));
  file.close();

  return CreateFromMemory(device, code);
}

auto ShaderModule::CreateFromMemory(VkDevice device,
                                    std::span<const uint32_t> code)
    -> std::expected<ShaderModule, std::string> {
  ShaderModule sm;
  sm.device_ = device;

  VkShaderModuleCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  info.codeSize = code.size() * sizeof(uint32_t);
  info.pCode = code.data();

  if (vkCreateShaderModule(device, &info, nullptr, &sm.module_) !=
      VK_SUCCESS) {
    return std::unexpected("Failed to create shader module");
  }

  return sm;
}

ShaderModule::~ShaderModule() {
  if (module_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
    vkDestroyShaderModule(device_, module_, nullptr);
  }
}

ShaderModule::ShaderModule(ShaderModule&& other) noexcept
    : device_(other.device_), module_(other.module_) {
  other.device_ = VK_NULL_HANDLE;
  other.module_ = VK_NULL_HANDLE;
}

ShaderModule& ShaderModule::operator=(ShaderModule&& other) noexcept {
  if (this != &other) {
    if (module_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
      vkDestroyShaderModule(device_, module_, nullptr);
    }
    device_ = other.device_;
    module_ = other.module_;
    other.device_ = VK_NULL_HANDLE;
    other.module_ = VK_NULL_HANDLE;
  }
  return *this;
}

}  // namespace graphics
