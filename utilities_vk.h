/* Copyright (c) 2020, NVIDIA CORPORATION. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *  * Neither the name of NVIDIA CORPORATION nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#pragma once

// Contains utility classes for this sample.
// Many of these are specific to this sample and wouldn't fit in the more
// general NVVK helper library - for instance, Vertex specifies the vertex
// binding description and attribute description for the geometry that this
// sample specifically uses.

#if defined(_WIN32) && (!defined(VK_USE_PLATFORM_WIN32_KHR))
#define VK_USE_PLATFORM_WIN32_KHR
#endif  // #if defined(_WIN32) && (!defined(VK_USE_PLATFORM_WIN32_KHR))
#pragma warning(disable : 26812)  // Disable the warning about Vulkan's enumerations being untyped in VS2019.

#include <array>
#include <nvh/geometry.hpp>
#include <nvmath/nvmath.h>
#include <nvmath/nvmath_glsltypes.h>
#include <nvvk/allocator_dma_vk.hpp>
#include <nvvk/buffers_vk.hpp>
#include <nvvk/context_vk.hpp>
#include <nvvk/debug_util_vk.hpp>
#include <nvvk/structs_vk.hpp>
#include <vulkan/vulkan_core.h>

// Vertex structure used for the main mesh.
struct Vertex
{
  nvmath::vec3 pos;
  nvmath::vec3 normal;
  nvmath::vec4 color;

  // Must have a constructor from nvh::geometry::Vertex in order for initScene
  // to work
  Vertex(const nvh::geometry::Vertex& vertex)
  {
    for(int i = 0; i < 3; i++)
    {
      pos[i]    = vertex.position[i];
      normal[i] = vertex.normal[i];
    }
    color = nvmath::vec4(1.0f);
  }

  static VkVertexInputBindingDescription getBindingDescription()
  {
    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding                         = 0;
    bindingDescription.stride                          = sizeof(Vertex);
    bindingDescription.inputRate                       = VK_VERTEX_INPUT_RATE_VERTEX;

    return bindingDescription;
  }

  static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions()
  {
    std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = {};

    attributeDescriptions[0].binding  = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset   = offsetof(Vertex, pos);

    attributeDescriptions[1].binding  = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset   = offsetof(Vertex, normal);

    attributeDescriptions[2].binding  = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format   = VK_FORMAT_R32G32B32A32_SFLOAT;
    attributeDescriptions[2].offset   = offsetof(Vertex, color);

    return attributeDescriptions;
  }
};

// A BufferAndView is an NVVK buffer (i.e. Vulkan buffer and underlying memory),
// together with a view that points to the whole buffer. It's a simplification
// that works for this sample!
struct BufferAndView
{
  nvvk::BufferDma buffer;
  VkBufferView    view = nullptr;
  VkDeviceSize    size = 0;  // In bytes

  // Creates a buffer and view with the given size, usage, and view format.
  // The memory properties are always VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT.
  void create(nvvk::Context& context, nvvk::AllocatorDma& allocator, VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage, VkFormat viewFormat)
  {
    assert(buffer.buffer == nullptr);  // Destroy the buffer before recreating it, please!
    buffer = allocator.createBuffer(bufferSize, bufferUsage);
    if((bufferUsage & (VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT)) != 0)
    {
      view = nvvk::createBufferView(context, nvvk::makeBufferViewCreateInfo(buffer.buffer, viewFormat, bufferSize));
    }
    size = bufferSize;
  }

  // To destroy the object, provide its context and allocator.
  void destroy(nvvk::Context& context, nvvk::AllocatorDma& allocator)
  {
    if(buffer.buffer != nullptr)
    {
      allocator.destroy(buffer);
    }

    if(view != nullptr)
    {
      vkDestroyBufferView(context.m_device, view, nullptr);
      view = nullptr;
    }

    size = 0;
  }

  void setName(nvvk::DebugUtil& util, const char* name)
  {
    util.setObjectName(buffer.buffer, name);
    if(view != nullptr)
    {
      util.setObjectName(view, name);
    }
  }

  // Attempts to ensure that all memory read/write operations involving the
  // given buffer have completed by creating a VkCmdPipelineBarrier with
  // a VkBufferMemoryBarrier that doesn't change anything.
  // VkBufferMemoryBarriers can target specific parts of buffers, but here we
  // depend upon the entire buffer.
  void memoryBarrier(VkCommandBuffer      cmdBuffer,
                     VkPipelineStageFlags stagesDependedUpon,
                     VkPipelineStageFlags stagesThatDepend,
                     VkAccessFlags        accessesDependedUpon,
                     VkAccessFlags        accessesThatDepend)
  {
    VkBufferMemoryBarrier barrier = nvvk::make<VkBufferMemoryBarrier>();
    barrier.srcAccessMask         = stagesDependedUpon;
    barrier.dstAccessMask         = stagesThatDepend;
    barrier.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
    barrier.buffer                = buffer.buffer;
    barrier.offset                = 0;
    barrier.size                  = size;

    vkCmdPipelineBarrier(cmdBuffer, stagesDependedUpon, stagesThatDepend, 0, 0, nullptr, 1, &barrier, 0, nullptr);
  }
};

// Creates a simple texture with 1 mip, 1 array layer, 1 sample per texel, with
// optimal tiling, in an undefined layout, with the VK_IMAGE_USAGE_SAMPLED_BIT flag
// (and possibly additional flags), and accessible only from a single queue family.
inline nvvk::ImageDma createImageSimple(nvvk::AllocatorDma& allocator,
                                 VkImageType         imageType,
                                 VkFormat            format,
                                 uint32_t            width,
                                 uint32_t            height,
                                 uint32_t            arrayLayers          = 1,
                                 VkImageUsageFlags   additionalUsageFlags = 0,
                                 uint32_t            numSamples           = 1)
{
  // There are several different ways to create images using the NVVK framework.
  // Here, we'll use AllocatorDma::createImage.

  VkImageCreateInfo imageInfo = nvvk::make<VkImageCreateInfo>();
  imageInfo.imageType         = imageType;
  imageInfo.extent.width      = width;
  imageInfo.extent.height     = height;
  imageInfo.extent.depth      = 1;
  imageInfo.mipLevels         = 1;
  imageInfo.arrayLayers       = arrayLayers;
  imageInfo.format            = format;
  imageInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage             = VK_IMAGE_USAGE_SAMPLED_BIT | additionalUsageFlags;
  imageInfo.samples           = static_cast<VkSampleCountFlagBits>(numSamples);
  imageInfo.sharingMode       = VK_SHARING_MODE_EXCLUSIVE;

  return allocator.createImage(imageInfo);
}

// A simple wrapper for writing a vkCmdPipelineBarrier for doing things such as
// image layout transitions.
inline void doTransition(VkCommandBuffer      cmdBuffer,
                  VkImage              image,
                  VkImageAspectFlags   aspect,
                  VkImageLayout        srcLayout,
                  VkPipelineStageFlags srcStages,
                  VkAccessFlags        srcAccesses,
                  VkImageLayout        dstLayout,    // How the image will be laid out in memory.
                  VkPipelineStageFlags dstStages,    // The stages that the image will be accessible from
                  VkAccessFlags        dstAccesses,  // The ways that the app will be able to access the image.
                  uint32_t             numLayers)                // The number of array layers in the image.
{
  VkImageMemoryBarrier barrier            = nvvk::make<VkImageMemoryBarrier>();
  barrier.oldLayout                       = srcLayout;
  barrier.newLayout                       = dstLayout;
  barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
  barrier.image                           = image;
  barrier.subresourceRange.aspectMask     = aspect;
  barrier.subresourceRange.baseMipLevel   = 0;
  barrier.subresourceRange.levelCount     = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount     = numLayers;
  barrier.srcAccessMask                   = srcAccesses;
  barrier.dstAccessMask                   = dstAccesses;

  vkCmdPipelineBarrier(cmdBuffer, srcStages, dstStages, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

// An ImageAndView is an NVVK image (i.e. Vulkan image and underlying memory),
// together with a view that points to the whole image, and data to track its
// current state. It's a simplification that works for this sample!
struct ImageAndView
{
  nvvk::ImageDma image;
  VkImageView    view = nullptr;
  // Information you might need, but please don't modify
  uint32_t c_width  = 0;                    // Should not be changed once the texture is created!
  uint32_t c_height = 0;                    // Should not be changed once the texture is created!
  uint32_t c_layers = 0;                    // Should not be changed once the texture is created!
  VkFormat c_format = VK_FORMAT_UNDEFINED;  // Should not be changed once the texture is created!

  // Information for pipeline transitions. These should generally only be
  // modified via transitionTo or when ending render passes.
  VkImageLayout        currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // The current layout of the image in GPU memory (e.g. GENERAL or COLOR_ATTACHMENT_OPTIMAL)
  VkPipelineStageFlags currentStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;  // The set of stages that this texture may be bound to (e.g. TOP_OF_PIPE or FRAGMENT_SHADER)
  VkAccessFlags        currentAccesses = 0;  // How the memory of this texture can be accessed (e.g. shader read/write, color attachment read/write)

public:
  // Creates a simple texture and view with 1 mip and 1 array layer.
  // with optimal tiling, in an undefined layout, with the VK_IMAGE_USAGE_SAMPLED_BIT flag
  // (and possibly additional flags), and accessible only from a single queue family.
  void create(nvvk::Context&      context,
              nvvk::AllocatorDma& allocator,
              VkImageType         imageType,
              VkImageAspectFlags  viewAspect,
              VkFormat            format,
              uint32_t            width,
              uint32_t            height,
              uint32_t            arrayLayers          = 1,
              VkImageUsageFlags   additionalUsageFlags = 0,
              uint32_t            numSamples           = 1)
  {
    assert(view == nullptr);  // Destroy the image before recreating it, please!
    image = createImageSimple(allocator, imageType, format, width, height, arrayLayers, additionalUsageFlags, numSamples);
    VkImageViewCreateInfo viewInfo       = nvvk::makeImage2DViewCreateInfo(image.image, format, viewAspect);
    viewInfo.subresourceRange.layerCount = arrayLayers;
    viewInfo.viewType                    = (arrayLayers == 1 ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY);
    vkCreateImageView(context.m_device, &viewInfo, nullptr, &view);

    c_width  = width;
    c_height = height;
    c_layers = arrayLayers;
    c_format = format;
  }

  // To destroy the object, provide its context and allocator.
  void destroy(nvvk::Context& context, nvvk::AllocatorDma& allocator)
  {
    if(view != nullptr)
    {
      vkDestroyImageView(context.m_device, view, nullptr);
      allocator.destroy(image);
      view            = nullptr;
      currentLayout   = VK_IMAGE_LAYOUT_UNDEFINED;
      currentStages   = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
      currentAccesses = 0;
    }
  }

  void transitionTo(VkCommandBuffer      cmdBuffer,
                    VkImageLayout        dstLayout,  // How the image will be laid out in memory.
                    VkPipelineStageFlags dstStages,  // The stages that the image will be accessible from
                    VkAccessFlags        dstAccesses)       // The ways that the app will be able to access the image.
  {
    // Note that in larger applications, we could batch together pipeline
    // barriers for better performance!

    // Maps to barrier.subresourceRange.aspectMask
    VkImageAspectFlags aspectMask = 0;
    if(dstLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
      aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
      if(c_format == VK_FORMAT_D32_SFLOAT_S8_UINT || c_format == VK_FORMAT_D24_UNORM_S8_UINT)
      {
        aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
      }
    }
    else
    {
      aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    doTransition(cmdBuffer, image.image, aspectMask, currentLayout, currentStages, currentAccesses, dstLayout,
                 dstStages, dstAccesses, c_layers);

    // Update current layout, stages, and accesses
    currentLayout   = dstLayout;
    currentStages   = dstStages;
    currentAccesses = dstAccesses;
  }

  // Should be called to keep track of the image's current layout when a render
  // pass that includes a image layout transition finishes.
  void endRenderPass(VkImageLayout dstLayout) { currentLayout = dstLayout; }

  void setName(nvvk::DebugUtil& util, const char* name)
  {
    util.setObjectName(image.image, name);
    util.setObjectName(view, name);
  }
};