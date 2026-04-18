#pragma once

#include <expected>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

namespace graphics {

class VulkanContext;
class Swapchain;

// Owns a graphics pipeline, render pass, and framebuffers for a single pass.
class Pipeline {
 public:
  [[nodiscard]] static auto Create(const VulkanContext& context,
                                   const Swapchain& swapchain,
                                   const std::string& vertex_shader_path,
                                   const std::string& fragment_shader_path)
      -> std::expected<Pipeline, std::string>;

  ~Pipeline();

  Pipeline(const Pipeline&) = delete;
  Pipeline& operator=(const Pipeline&) = delete;
  Pipeline(Pipeline&& other) noexcept;
  Pipeline& operator=(Pipeline&& other) noexcept;

  [[nodiscard]] auto Handle() const -> VkPipeline { return pipeline_; }
  [[nodiscard]] auto RenderPass() const -> VkRenderPass {
    return render_pass_;
  }
  [[nodiscard]] auto Layout() const -> VkPipelineLayout { return layout_; }
  [[nodiscard]] auto Framebuffers() const
      -> const std::vector<VkFramebuffer>& { return framebuffers_; }

 private:
  Pipeline() = default;

  VkDevice device_ = VK_NULL_HANDLE;  // non-owning reference
  VkPipeline pipeline_ = VK_NULL_HANDLE;
  VkPipelineLayout layout_ = VK_NULL_HANDLE;
  VkRenderPass render_pass_ = VK_NULL_HANDLE;
  std::vector<VkFramebuffer> framebuffers_;
};

}  // namespace graphics
