#pragma once

#include <expected>
#include <span>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

namespace graphics {

// Loads a SPIR-V binary and creates a VkShaderModule. RAII — destroyed on scope exit.
class ShaderModule {
 public:
  [[nodiscard]] static auto CreateFromFile(VkDevice device,
                                           const std::string& spirv_path)
      -> std::expected<ShaderModule, std::string>;

  [[nodiscard]] static auto CreateFromMemory(VkDevice device,
                                             std::span<const uint32_t> code)
      -> std::expected<ShaderModule, std::string>;

  ~ShaderModule();

  ShaderModule(const ShaderModule&) = delete;
  ShaderModule& operator=(const ShaderModule&) = delete;
  ShaderModule(ShaderModule&& other) noexcept;
  ShaderModule& operator=(ShaderModule&& other) noexcept;

  [[nodiscard]] auto Handle() const -> VkShaderModule { return module_; }

 private:
  ShaderModule() = default;

  VkDevice device_ = VK_NULL_HANDLE;
  VkShaderModule module_ = VK_NULL_HANDLE;
};

}  // namespace graphics
