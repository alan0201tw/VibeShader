#include "graphics/pipeline.h"

#include "graphics/shader_module.h"
#include "graphics/swapchain.h"
#include "graphics/vulkan_context.h"

namespace graphics {

auto Pipeline::Create(const VulkanContext& context,
                      const Swapchain& swapchain,
                      const std::string& vertex_shader_path,
                      const std::string& fragment_shader_path)
    -> std::expected<Pipeline, std::string> {
  Pipeline pl;
  pl.device_ = context.Device();

  // ── Load shaders ────────────────────────────────────────────────────────
  auto vert_result =
      ShaderModule::CreateFromFile(pl.device_, vertex_shader_path);
  if (!vert_result) return std::unexpected(vert_result.error());

  auto frag_result =
      ShaderModule::CreateFromFile(pl.device_, fragment_shader_path);
  if (!frag_result) return std::unexpected(frag_result.error());

  VkPipelineShaderStageCreateInfo stages[2]{};
  stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = vert_result->Handle();
  stages[0].pName = "main";

  stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = frag_result->Handle();
  stages[1].pName = "main";

  // ── Vertex input (hardcoded triangle — no vertex buffer) ────────────────
  VkPipelineVertexInputStateCreateInfo vertex_input{};
  vertex_input.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  VkPipelineInputAssemblyStateCreateInfo input_assembly{};
  input_assembly.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  input_assembly.primitiveRestartEnable = VK_FALSE;

  // ── Dynamic viewport and scissor ───────────────────────────────────────
  VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT,
                                     VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic_state{};
  dynamic_state.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state.dynamicStateCount = 2;
  dynamic_state.pDynamicStates = dynamic_states;

  VkPipelineViewportStateCreateInfo viewport_state{};
  viewport_state.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state.viewportCount = 1;
  viewport_state.scissorCount = 1;

  // ── Rasterizer ──────────────────────────────────────────────────────────
  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = VK_CULL_MODE_NONE;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.depthBiasEnable = VK_FALSE;

  // ── Multisampling (disabled) ────────────────────────────────────────────
  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  // ── Color blending ─────────────────────────────────────────────────────
  VkPipelineColorBlendAttachmentState color_blend_attachment{};
  color_blend_attachment.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  color_blend_attachment.blendEnable = VK_FALSE;

  VkPipelineColorBlendStateCreateInfo color_blending{};
  color_blending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blending.logicOpEnable = VK_FALSE;
  color_blending.attachmentCount = 1;
  color_blending.pAttachments = &color_blend_attachment;

  // ── Pipeline layout ─────────────────────────────────────────────────────
  VkPushConstantRange push_constant_range{};
  push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
  push_constant_range.offset = 0;
  push_constant_range.size = sizeof(float) * 2;  // time + aspect ratio

  VkPipelineLayoutCreateInfo layout_info{};
  layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layout_info.pushConstantRangeCount = 1;
  layout_info.pPushConstantRanges = &push_constant_range;

  if (vkCreatePipelineLayout(pl.device_, &layout_info, nullptr,
                             &pl.layout_) != VK_SUCCESS) {
    return std::unexpected("Failed to create pipeline layout");
  }

  // ── Render pass ─────────────────────────────────────────────────────────
  VkAttachmentDescription color_attachment{};
  color_attachment.format = swapchain.Format();
  color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference color_ref{};
  color_ref.attachment = 0;
  color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass{};
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &color_ref;

  VkSubpassDependency dependency{};
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstStageMask =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo render_pass_info{};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_info.attachmentCount = 1;
  render_pass_info.pAttachments = &color_attachment;
  render_pass_info.subpassCount = 1;
  render_pass_info.pSubpasses = &subpass;
  render_pass_info.dependencyCount = 1;
  render_pass_info.pDependencies = &dependency;

  if (vkCreateRenderPass(pl.device_, &render_pass_info, nullptr,
                         &pl.render_pass_) != VK_SUCCESS) {
    return std::unexpected("Failed to create render pass");
  }

  // ── Graphics pipeline ───────────────────────────────────────────────────
  VkGraphicsPipelineCreateInfo pipeline_info{};
  pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_info.stageCount = 2;
  pipeline_info.pStages = stages;
  pipeline_info.pVertexInputState = &vertex_input;
  pipeline_info.pInputAssemblyState = &input_assembly;
  pipeline_info.pViewportState = &viewport_state;
  pipeline_info.pRasterizationState = &rasterizer;
  pipeline_info.pMultisampleState = &multisampling;
  pipeline_info.pColorBlendState = &color_blending;
  pipeline_info.pDynamicState = &dynamic_state;
  pipeline_info.layout = pl.layout_;
  pipeline_info.renderPass = pl.render_pass_;
  pipeline_info.subpass = 0;

  if (vkCreateGraphicsPipelines(pl.device_, VK_NULL_HANDLE, 1,
                                &pipeline_info, nullptr,
                                &pl.pipeline_) != VK_SUCCESS) {
    return std::unexpected("Failed to create graphics pipeline");
  }

  // ── Framebuffers ────────────────────────────────────────────────────────
  const auto& image_views = swapchain.ImageViews();
  pl.framebuffers_.resize(image_views.size());

  for (size_t i = 0; i < image_views.size(); ++i) {
    VkFramebufferCreateInfo fb_info{};
    fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fb_info.renderPass = pl.render_pass_;
    fb_info.attachmentCount = 1;
    fb_info.pAttachments = &image_views[i];
    fb_info.width = swapchain.Extent().width;
    fb_info.height = swapchain.Extent().height;
    fb_info.layers = 1;

    if (vkCreateFramebuffer(pl.device_, &fb_info, nullptr,
                            &pl.framebuffers_[i]) != VK_SUCCESS) {
      return std::unexpected("Failed to create framebuffer");
    }
  }

  return pl;
}

Pipeline::~Pipeline() {
  if (device_ == VK_NULL_HANDLE) return;
  for (auto fb : framebuffers_) {
    vkDestroyFramebuffer(device_, fb, nullptr);
  }
  if (pipeline_ != VK_NULL_HANDLE) {
    vkDestroyPipeline(device_, pipeline_, nullptr);
  }
  if (layout_ != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(device_, layout_, nullptr);
  }
  if (render_pass_ != VK_NULL_HANDLE) {
    vkDestroyRenderPass(device_, render_pass_, nullptr);
  }
}

Pipeline::Pipeline(Pipeline&& other) noexcept
    : device_(other.device_),
      pipeline_(other.pipeline_),
      layout_(other.layout_),
      render_pass_(other.render_pass_),
      framebuffers_(std::move(other.framebuffers_)) {
  other.device_ = VK_NULL_HANDLE;
  other.pipeline_ = VK_NULL_HANDLE;
  other.layout_ = VK_NULL_HANDLE;
  other.render_pass_ = VK_NULL_HANDLE;
}

Pipeline& Pipeline::operator=(Pipeline&& other) noexcept {
  if (this != &other) {
    for (auto fb : framebuffers_) {
      vkDestroyFramebuffer(device_, fb, nullptr);
    }
    if (pipeline_ != VK_NULL_HANDLE)
      vkDestroyPipeline(device_, pipeline_, nullptr);
    if (layout_ != VK_NULL_HANDLE)
      vkDestroyPipelineLayout(device_, layout_, nullptr);
    if (render_pass_ != VK_NULL_HANDLE)
      vkDestroyRenderPass(device_, render_pass_, nullptr);

    device_ = other.device_;
    pipeline_ = other.pipeline_;
    layout_ = other.layout_;
    render_pass_ = other.render_pass_;
    framebuffers_ = std::move(other.framebuffers_);

    other.device_ = VK_NULL_HANDLE;
    other.pipeline_ = VK_NULL_HANDLE;
    other.layout_ = VK_NULL_HANDLE;
    other.render_pass_ = VK_NULL_HANDLE;
  }
  return *this;
}

}  // namespace graphics
