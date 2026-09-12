/*
 * MIT License
 *
 * Copyright (c) 2025-2026 Soft Sprint Studios
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef VK_RAD_H
#define VK_RAD_H

#include "datatypes.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <string>

struct gpu_ray_t
{
    Float origin[3];
    Float tMin;
    Float dir[3];
    Float tMax;
};

struct gpu_ray_hit_t
{
    Float normal[3];
    Float dist;
    Uint32 primID;
    Float u;
    Float v;
    Uint32 hit;
};

struct gpu_leaf_sample_t
{
    Float mins[4];
    Float maxs[4];
    Float points[13][4];
    Int32 count;
    Int32 solid;
    Int32 pad[2];
};

struct vk_buffer_t
{
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceAddress deviceAddress = 0;
    VkDeviceSize size = 0;
};

struct vk_as_t
{
    VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
    vk_buffer_t buffer;
};

class CVulkanRayTracer
{
public:
    CVulkanRayTracer();
    ~CVulkanRayTracer();

    bool Initialize();
    void Shutdown();

    bool BuildSceneBVH(const std::vector<Float>& vertices, const std::vector<Uint32>& indices);

    bool RunPVSCompute(const std::vector<gpu_leaf_sample_t>& leafs, Uint32 numVisLeafs, std::vector<byte>& outPvsMatrix, Uint32 rowBytes);
    bool TraceOcclusionBatch(const std::vector<gpu_ray_t>& rays, std::vector<Uint32>& outHits);
    const gpu_ray_hit_t* TraceRayHitBatch(const std::vector<gpu_ray_t>& rays);

private:
    void EnsureBuffer(vk_buffer_t& buf, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    bool CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, vk_buffer_t& outBuffer);
    void DestroyBuffer(vk_buffer_t& buffer);
    Uint32 FindMemoryType(Uint32 typeFilter, VkMemoryPropertyFlags properties);

    VkShaderModule LoadSPIRV(const std::string& filename);
    bool CreateComputePipeline(VkShaderModule shaderModule, VkDescriptorSetLayout& outDescLayout, VkPipelineLayout& outPipeLayout, VkPipeline& outPipeline);

    VkInstance m_instance = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_queue = VK_NULL_HANDLE;
    Uint32 m_queueFamilyIndex = 0;

    VkCommandPool m_cmdPool = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;

    vk_as_t m_blas;
    vk_as_t m_tlas;

    VkDescriptorSetLayout m_pvsDescLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_pvsPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pvsPipeline = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_occludeDescLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_occludePipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_occludePipeline = VK_NULL_HANDLE;

    VkDescriptorSetLayout m_intersectDescLayout = VK_NULL_HANDLE;
    VkPipelineLayout m_intersectPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_intersectPipeline = VK_NULL_HANDLE;

    vk_buffer_t m_rayBuf;
    vk_buffer_t m_hitBuf;
    void* m_hitMapped = nullptr;

    PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddressKHR_fn = nullptr;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR_fn = nullptr;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR_fn = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR_fn = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR_fn = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR_fn = nullptr;
};

#endif