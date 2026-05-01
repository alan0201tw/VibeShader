#include "graphics/renderer.h"

#include <format>
#include <print>

#include "graphics/pipeline.h"
#include "graphics/swapchain.h"
#include "graphics/vulkan_context.h"

namespace graphics {

auto Renderer::Create(const VulkanContext& context,
                      const Swapchain& swapchain)
    -> std::expected<Renderer, std::string> {
  Renderer renderer;
  renderer.device_ = context.Device();
  renderer.graphics_queue_ = context.GraphicsQueue();
  renderer.present_queue_ = context.PresentQueue();
  renderer.swapchain_ = swapchain.Handle();
  renderer.swapchain_extent_ = swapchain.Extent();

  // ── Command pool ───────────────────────────────────────────────────────
  VkCommandPoolCreateInfo pool_info{};
  pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  pool_info.queueFamilyIndex = context.GraphicsQueueFamily();

  if (vkCreateCommandPool(renderer.device_, &pool_info, nullptr,
                          &renderer.command_pool_) != VK_SUCCESS) {
    return std::unexpected("Failed to create command pool");
  }

  // ── Command buffers ────────────────────────────────────────────────────
  auto image_count = static_cast<uint32_t>(swapchain.ImageViews().size());
  renderer.command_buffers_.resize(image_count);

  VkCommandBufferAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  alloc_info.commandPool = renderer.command_pool_;
  alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  alloc_info.commandBufferCount = image_count;

  if (vkAllocateCommandBuffers(renderer.device_, &alloc_info,
                               renderer.command_buffers_.data()) !=
      VK_SUCCESS) {
    return std::unexpected("Failed to allocate command buffers");
  }

  // ── Synchronization primitives ──────────────────────────────────────────
  renderer.image_available_semaphores_.resize(image_count);
  renderer.render_finished_semaphores_.resize(image_count);
  renderer.in_flight_fences_.resize(image_count);

  VkSemaphoreCreateInfo semaphore_info{};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fence_info{};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // Start signaled

  for (uint32_t i = 0; i < image_count; ++i) {
    if (vkCreateSemaphore(renderer.device_, &semaphore_info, nullptr,
                          &renderer.image_available_semaphores_[i]) !=
        VK_SUCCESS) {
      return std::unexpected("Failed to create image available semaphores");
    }

    if (vkCreateSemaphore(renderer.device_, &semaphore_info, nullptr,
                          &renderer.render_finished_semaphores_[i]) !=
        VK_SUCCESS) {
      return std::unexpected("Failed to create render finished semaphores");
    }

    if (vkCreateFence(renderer.device_, &fence_info, nullptr,
                      &renderer.in_flight_fences_[i]) != VK_SUCCESS) {
      return std::unexpected("Failed to create in-flight fences");
    }
  }

  return renderer;
}

Renderer::~Renderer() {
  if (device_ == VK_NULL_HANDLE) {
    return;
  }
  vkDeviceWaitIdle(device_);
  for (auto fence : in_flight_fences_) {
    if (fence != VK_NULL_HANDLE) {
      vkDestroyFence(device_, fence, nullptr);
    }
  }
  for (auto semaphore : render_finished_semaphores_) {
    if (semaphore != VK_NULL_HANDLE) {
      vkDestroySemaphore(device_, semaphore, nullptr);
    }
  }
  for (auto semaphore : image_available_semaphores_) {
    if (semaphore != VK_NULL_HANDLE) {
      vkDestroySemaphore(device_, semaphore, nullptr);
    }
  }
  if (command_pool_ != VK_NULL_HANDLE) {
    vkDestroyCommandPool(device_, command_pool_, nullptr);
  }
}

Renderer::Renderer(Renderer&& other) noexcept
    : device_(other.device_),
      graphics_queue_(other.graphics_queue_),
      present_queue_(other.present_queue_),
      swapchain_(other.swapchain_),
  swapchain_extent_(other.swapchain_extent_),
      command_pool_(other.command_pool_),
      command_buffers_(std::move(other.command_buffers_)),
      image_available_semaphores_(std::move(other.image_available_semaphores_)),
      render_finished_semaphores_(std::move(other.render_finished_semaphores_)),
      in_flight_fences_(std::move(other.in_flight_fences_)) {
  other.device_ = VK_NULL_HANDLE;
  other.command_pool_ = VK_NULL_HANDLE;
}

Renderer& Renderer::operator=(Renderer&& other) noexcept {
  if (this != &other) {
    // Cleanup current resources
    if (device_ != VK_NULL_HANDLE) {
      vkDeviceWaitIdle(device_);
      for (auto fence : in_flight_fences_) {
        if (fence != VK_NULL_HANDLE) {
          vkDestroyFence(device_, fence, nullptr);
        }
      }
      for (auto semaphore : render_finished_semaphores_) {
        if (semaphore != VK_NULL_HANDLE) {
          vkDestroySemaphore(device_, semaphore, nullptr);
        }
      }
      for (auto semaphore : image_available_semaphores_) {
        if (semaphore != VK_NULL_HANDLE) {
          vkDestroySemaphore(device_, semaphore, nullptr);
        }
      }
      if (command_pool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device_, command_pool_, nullptr);
      }
    }

    device_ = other.device_;
    graphics_queue_ = other.graphics_queue_;
    present_queue_ = other.present_queue_;
    swapchain_ = other.swapchain_;
    swapchain_extent_ = other.swapchain_extent_;
    command_pool_ = other.command_pool_;
    command_buffers_ = std::move(other.command_buffers_);
    image_available_semaphores_ = std::move(other.image_available_semaphores_);
    render_finished_semaphores_ = std::move(other.render_finished_semaphores_);
    in_flight_fences_ = std::move(other.in_flight_fences_);

    other.device_ = VK_NULL_HANDLE;
    other.command_pool_ = VK_NULL_HANDLE;
  }
  return *this;
}

auto Renderer::BeginFrame() -> std::expected<uint32_t, std::string> {
  // Get current frame's synchronization primitives
  auto current_fence = in_flight_fences_[current_frame_index_];
  auto current_image_available = image_available_semaphores_[current_frame_index_];

  // Wait for previous frame to complete
  if (vkWaitForFences(device_, 1, &current_fence, VK_TRUE, UINT64_MAX) !=
      VK_SUCCESS) {
    return std::unexpected("Failed to wait for fence");
  }
  if (vkResetFences(device_, 1, &current_fence) != VK_SUCCESS) {
    return std::unexpected("Failed to reset fence");
  }

  // Acquire next swapchain image
  auto image_index = uint32_t{0};
  auto result =
      vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
                            current_image_available, VK_NULL_HANDLE,
                            &image_index);
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    return std::unexpected("Swapchain out of date");
  }
  if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    return std::unexpected(std::format(
        "Failed to acquire swapchain image: {}",
        static_cast<int>(result)));
  }

  return image_index;
}

void Renderer::RecordRenderPass(const Pipeline& pipeline,
                                uint32_t image_index, float time,
                                float aspect_ratio) {
  VkCommandBuffer cmd = command_buffers_[image_index];

  // Begin recording
  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cmd, &begin_info);

  // ── Render pass ─────────────────────────────────────────────────────────
  VkClearValue clear_color{};
  clear_color.color.float32[0] = 0.0f;
  clear_color.color.float32[1] = 0.0f;
  clear_color.color.float32[2] = 0.0f;
  clear_color.color.float32[3] = 1.0f;

  VkRenderPassBeginInfo render_pass_begin{};
  render_pass_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  render_pass_begin.renderPass = pipeline.RenderPass();
  render_pass_begin.framebuffer = pipeline.Framebuffers()[image_index];
  render_pass_begin.renderArea.offset = {0, 0};
  render_pass_begin.renderArea.extent = swapchain_extent_;
  render_pass_begin.clearValueCount = 1;
  render_pass_begin.pClearValues = &clear_color;

  vkCmdBeginRenderPass(cmd, &render_pass_begin,
                       VK_SUBPASS_CONTENTS_INLINE);

  // ── Bind pipeline and set push constants ───────────────────────────────
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.Handle());

  // ── Set dynamic viewport and scissor ────────────────────────────────────
  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(swapchain_extent_.width);
  viewport.height = static_cast<float>(swapchain_extent_.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = swapchain_extent_;
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  struct PushConstants {
    float time;
    float aspect;
  } pc{time, aspect_ratio};

  vkCmdPushConstants(cmd, pipeline.Layout(),
                     VK_SHADER_STAGE_VERTEX_BIT |
                         VK_SHADER_STAGE_FRAGMENT_BIT,
                     0, sizeof(pc), &pc);

  // ── Draw fullscreen triangle (3 vertices) ───────────────────────────────
  vkCmdDraw(cmd, 3, 1, 0, 0);

  vkCmdEndRenderPass(cmd);
  vkEndCommandBuffer(cmd);
}

void Renderer::RecordRenderPass(const Pipeline& pipeline,
                                uint32_t image_index,
                                const void* push_constants, size_t push_constant_size,
                                VkDescriptorSet mesh_descriptor_set) {
  VkCommandBuffer cmd = command_buffers_[image_index];

  // Begin recording
  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkBeginCommandBuffer(cmd, &begin_info);

  // ── Render pass ─────────────────────────────────────────────────────────
  VkClearValue clear_color{};
  clear_color.color.float32[0] = 0.0f;
  clear_color.color.float32[1] = 0.0f;
  clear_color.color.float32[2] = 0.0f;
  clear_color.color.float32[3] = 1.0f;

  VkRenderPassBeginInfo render_pass_begin{};
  render_pass_begin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  render_pass_begin.renderPass = pipeline.RenderPass();
  render_pass_begin.framebuffer = pipeline.Framebuffers()[image_index];
  render_pass_begin.renderArea.offset = {0, 0};
  render_pass_begin.renderArea.extent = swapchain_extent_;
  render_pass_begin.clearValueCount = 1;
  render_pass_begin.pClearValues = &clear_color;

  vkCmdBeginRenderPass(cmd, &render_pass_begin,
                       VK_SUBPASS_CONTENTS_INLINE);

  // ── Bind pipeline and set push constants ───────────────────────────────
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.Handle());

  // ── Bind mesh descriptor set ───────────────────────────────────────────
  vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.Layout(), 0, 1, &mesh_descriptor_set, 0, nullptr);

  // ── Set dynamic viewport and scissor ────────────────────────────────────
  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = static_cast<float>(swapchain_extent_.width);
  viewport.height = static_cast<float>(swapchain_extent_.height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(cmd, 0, 1, &viewport);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = swapchain_extent_;
  vkCmdSetScissor(cmd, 0, 1, &scissor);

  vkCmdPushConstants(cmd, pipeline.Layout(),
                     VK_SHADER_STAGE_VERTEX_BIT |
                         VK_SHADER_STAGE_FRAGMENT_BIT,
                     0, static_cast<uint32_t>(push_constant_size), push_constants);

  // ── Draw fullscreen triangle (3 vertices) ───────────────────────────────
  vkCmdDraw(cmd, 3, 1, 0, 0);

  vkCmdEndRenderPass(cmd);
  vkEndCommandBuffer(cmd);
}

auto Renderer::EndFrameAndPresent(uint32_t image_index)
    -> std::expected<void, std::string> {
  VkCommandBuffer cmd = command_buffers_[image_index];

  // Use current frame's synchronization primitives
  VkFence current_fence = in_flight_fences_[current_frame_index_];
  VkSemaphore current_image_available = image_available_semaphores_[current_frame_index_];
  VkSemaphore current_render_finished = render_finished_semaphores_[current_frame_index_];

  // ── Submit command buffer ───────────────────────────────────────────────
  VkPipelineStageFlags wait_stages =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = &current_image_available;
  submit_info.pWaitDstStageMask = &wait_stages;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &cmd;
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &current_render_finished;

  if (vkQueueSubmit(graphics_queue_, 1, &submit_info, current_fence) !=
      VK_SUCCESS) {
    return std::unexpected("Failed to submit graphics queue");
  }

  // ── Present to surface ──────────────────────────────────────────────────
  VkPresentInfoKHR present_info{};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &current_render_finished;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &swapchain_;
  present_info.pImageIndices = &image_index;

  VkResult result = vkQueuePresentKHR(present_queue_, &present_info);
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    return std::unexpected("Swapchain out of date");
  }
  if (result != VK_SUCCESS) {
    return std::unexpected(
        std::format("Failed to present: {}", static_cast<int>(result)));
  }

  // Advance to next frame
  current_frame_index_ = (current_frame_index_ + 1) % image_available_semaphores_.size();

  return {};
}

}  // namespace graphics
