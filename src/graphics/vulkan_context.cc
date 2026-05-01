#include "graphics/vulkan_context.h"

#include <cstring>
#include <print>
#include <vector>

#include <GLFW/glfw3.h>

namespace graphics {
namespace {

constexpr const char* kValidationLayerName =
    "VK_LAYER_KHRONOS_validation";

#ifdef NDEBUG
constexpr bool kEnableValidation = false;
#else
constexpr bool kEnableValidation = true;
#endif

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* /*user_data*/) {
  if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
    std::println(stderr, "[Vulkan] {}", callback_data->pMessage);
  }
  return VK_FALSE;
}

auto CreateDebugMessenger(VkInstance instance)
    -> VkDebugUtilsMessengerEXT {
  VkDebugUtilsMessengerCreateInfoEXT info{};
  info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  info.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  info.messageType =
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  info.pfnUserCallback = DebugCallback;

  auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
      vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
  VkDebugUtilsMessengerEXT messenger = VK_NULL_HANDLE;
  if (func != nullptr) {
    func(instance, &info, nullptr, &messenger);
  }
  return messenger;
}

void DestroyDebugMessenger(VkInstance instance,
                           VkDebugUtilsMessengerEXT messenger) {
  if (messenger == VK_NULL_HANDLE) {
    return;
  }
  auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
      vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
  if (func != nullptr) {
    func(instance, messenger, nullptr);
  }
}

auto CheckValidationLayerSupport() -> bool {
  auto count = uint32_t{0};
  vkEnumerateInstanceLayerProperties(&count, nullptr);
  std::vector<VkLayerProperties> layers(count);
  vkEnumerateInstanceLayerProperties(&count, layers.data());

  for (const auto& layer : layers) {
    if (std::strcmp(layer.layerName, kValidationLayerName) == 0) {
      return true;
    }
  }
  return false;
}

struct QueueFamilyIndices {
  uint32_t graphics = UINT32_MAX;
  uint32_t present = UINT32_MAX;

  [[nodiscard]] auto IsComplete() const -> bool {
    return graphics != UINT32_MAX && present != UINT32_MAX;
  }
};

auto FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
    -> QueueFamilyIndices {
  QueueFamilyIndices indices;
  uint32_t count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
  std::vector<VkQueueFamilyProperties> families(count);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

  for (auto i = uint32_t{0}; i < count; ++i) {
    if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices.graphics = i;
    }
    VkBool32 present_support = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface,
                                         &present_support);
    if (present_support) {
      indices.present = i;
    }
    if (indices.IsComplete()) {
      break;
    }
  }
  return indices;
}

}  // namespace

auto VulkanContext::Create(GLFWwindow* window)
    -> std::expected<VulkanContext, std::string> {
  VulkanContext ctx;

  // ── Instance ────────────────────────────────────────────────────────────
  VkApplicationInfo app_info{};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pApplicationName = "Playground";
  app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
  app_info.pEngineName = "No Engine";
  app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  app_info.apiVersion = VK_API_VERSION_1_3;

  uint32_t glfw_ext_count = 0;
  const char** glfw_extensions =
      glfwGetRequiredInstanceExtensions(&glfw_ext_count);
  std::vector<const char*> extensions(glfw_extensions,
                                      glfw_extensions + glfw_ext_count);

  std::vector<const char*> layers;
  if (kEnableValidation && CheckValidationLayerSupport()) {
    layers.push_back(kValidationLayerName);
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  VkInstanceCreateInfo instance_info{};
  instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_info.pApplicationInfo = &app_info;
  instance_info.enabledExtensionCount =
      static_cast<uint32_t>(extensions.size());
  instance_info.ppEnabledExtensionNames = extensions.data();
  instance_info.enabledLayerCount = static_cast<uint32_t>(layers.size());
  instance_info.ppEnabledLayerNames = layers.data();

  if (vkCreateInstance(&instance_info, nullptr, &ctx.instance_) !=
      VK_SUCCESS) {
    return std::unexpected("Failed to create Vulkan instance");
  }

  // ── Debug messenger ─────────────────────────────────────────────────────
  if (kEnableValidation) {
    ctx.debug_messenger_ = CreateDebugMessenger(ctx.instance_);
  }

  // ── Surface ─────────────────────────────────────────────────────────────
  if (glfwCreateWindowSurface(ctx.instance_, window, nullptr,
                              &ctx.surface_) != VK_SUCCESS) {
    return std::unexpected("Failed to create window surface");
  }

  // ── Physical device ─────────────────────────────────────────────────────
  uint32_t device_count = 0;
  vkEnumeratePhysicalDevices(ctx.instance_, &device_count, nullptr);
  if (device_count == 0) {
    return std::unexpected("No Vulkan-capable GPU found");
  }
  std::vector<VkPhysicalDevice> devices(device_count);
  vkEnumeratePhysicalDevices(ctx.instance_, &device_count, devices.data());

  for (const auto& device : devices) {
    auto indices = FindQueueFamilies(device, ctx.surface_);
    if (indices.IsComplete()) {
      ctx.physical_device_ = device;
      ctx.graphics_queue_family_ = indices.graphics;
      ctx.present_queue_family_ = indices.present;
      break;
    }
  }
  if (ctx.physical_device_ == VK_NULL_HANDLE) {
    return std::unexpected("No suitable GPU found");
  }

  // ── Logical device ──────────────────────────────────────────────────────
  float queue_priority = 1.0f;
  std::vector<VkDeviceQueueCreateInfo> queue_infos;

  VkDeviceQueueCreateInfo gfx_queue_info{};
  gfx_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  gfx_queue_info.queueFamilyIndex = ctx.graphics_queue_family_;
  gfx_queue_info.queueCount = 1;
  gfx_queue_info.pQueuePriorities = &queue_priority;
  queue_infos.push_back(gfx_queue_info);

  if (ctx.present_queue_family_ != ctx.graphics_queue_family_) {
    VkDeviceQueueCreateInfo present_queue_info{};
    present_queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    present_queue_info.queueFamilyIndex = ctx.present_queue_family_;
    present_queue_info.queueCount = 1;
    present_queue_info.pQueuePriorities = &queue_priority;
    queue_infos.push_back(present_queue_info);
  }

  const char* device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  VkPhysicalDeviceFeatures features{};

  VkDeviceCreateInfo device_info{};
  device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_info.queueCreateInfoCount =
      static_cast<uint32_t>(queue_infos.size());
  device_info.pQueueCreateInfos = queue_infos.data();
  device_info.enabledExtensionCount = 1;
  device_info.ppEnabledExtensionNames = device_extensions;
  device_info.pEnabledFeatures = &features;

  if (vkCreateDevice(ctx.physical_device_, &device_info, nullptr,
                     &ctx.device_) != VK_SUCCESS) {
    return std::unexpected("Failed to create logical device");
  }

  vkGetDeviceQueue(ctx.device_, ctx.graphics_queue_family_, 0,
                   &ctx.graphics_queue_);
  vkGetDeviceQueue(ctx.device_, ctx.present_queue_family_, 0,
                   &ctx.present_queue_);

  VkPhysicalDeviceProperties props{};
  vkGetPhysicalDeviceProperties(ctx.physical_device_, &props);
  std::println("GPU: {}", props.deviceName);

  return ctx;
}

VulkanContext::~VulkanContext() {
  if (device_ != VK_NULL_HANDLE) {
    vkDestroyDevice(device_, nullptr);
  }
  if (surface_ != VK_NULL_HANDLE) {
    vkDestroySurfaceKHR(instance_, surface_, nullptr);
  }
  DestroyDebugMessenger(instance_, debug_messenger_);
  if (instance_ != VK_NULL_HANDLE) {
    vkDestroyInstance(instance_, nullptr);
  }
}

VulkanContext::VulkanContext(VulkanContext&& other) noexcept
    : instance_(other.instance_),
      surface_(other.surface_),
      physical_device_(other.physical_device_),
      device_(other.device_),
      graphics_queue_(other.graphics_queue_),
      present_queue_(other.present_queue_),
      graphics_queue_family_(other.graphics_queue_family_),
      present_queue_family_(other.present_queue_family_),
      debug_messenger_(other.debug_messenger_) {
  other.instance_ = VK_NULL_HANDLE;
  other.surface_ = VK_NULL_HANDLE;
  other.physical_device_ = VK_NULL_HANDLE;
  other.device_ = VK_NULL_HANDLE;
  other.graphics_queue_ = VK_NULL_HANDLE;
  other.present_queue_ = VK_NULL_HANDLE;
  other.debug_messenger_ = VK_NULL_HANDLE;
}

VulkanContext& VulkanContext::operator=(VulkanContext&& other) noexcept {
  if (this != &other) {
    // Destroy current resources
    if (device_ != VK_NULL_HANDLE) vkDestroyDevice(device_, nullptr);
    if (surface_ != VK_NULL_HANDLE)
      vkDestroySurfaceKHR(instance_, surface_, nullptr);
    DestroyDebugMessenger(instance_, debug_messenger_);
    if (instance_ != VK_NULL_HANDLE) vkDestroyInstance(instance_, nullptr);

    // Move
    instance_ = other.instance_;
    surface_ = other.surface_;
    physical_device_ = other.physical_device_;
    device_ = other.device_;
    graphics_queue_ = other.graphics_queue_;
    present_queue_ = other.present_queue_;
    graphics_queue_family_ = other.graphics_queue_family_;
    present_queue_family_ = other.present_queue_family_;
    debug_messenger_ = other.debug_messenger_;

    other.instance_ = VK_NULL_HANDLE;
    other.surface_ = VK_NULL_HANDLE;
    other.physical_device_ = VK_NULL_HANDLE;
    other.device_ = VK_NULL_HANDLE;
    other.graphics_queue_ = VK_NULL_HANDLE;
    other.present_queue_ = VK_NULL_HANDLE;
    other.debug_messenger_ = VK_NULL_HANDLE;
  }
  return *this;
}

void VulkanContext::WaitIdle() const {
  if (device_ != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(device_);
  }
}

}  // namespace graphics
