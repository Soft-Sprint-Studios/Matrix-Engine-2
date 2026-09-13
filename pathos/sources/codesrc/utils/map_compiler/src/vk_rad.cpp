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
#include "vk_rad.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <omp.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

//=============================================
// @brief
//
//=============================================
CVulkanRayTracer::CVulkanRayTracer()
{
}

//=============================================
// @brief
//
//=============================================
CVulkanRayTracer::~CVulkanRayTracer()
{
    Shutdown();
}

//=============================================
// @brief
//
//=============================================
Uint32 CVulkanRayTracer::FindMemoryType(Uint32 typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(m_physicalDevice, &memProperties);

    for (Uint32 i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    if (properties & VK_MEMORY_PROPERTY_HOST_CACHED_BIT)
    {
        VkMemoryPropertyFlags fallbackProps = (properties & ~VK_MEMORY_PROPERTY_HOST_CACHED_BIT) | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        for (Uint32 i = 0; i < memProperties.memoryTypeCount; i++)
        {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & fallbackProps) == fallbackProps)
            {
                return i;
            }
        }
    }

    return 0;
}

//=============================================
// @brief
//
//=============================================
bool CVulkanRayTracer::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, vk_buffer_t& outBuffer)
{
    outBuffer.size = size;

    VkBufferCreateInfo bufferInfo{ VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_device, &bufferInfo, nullptr, &outBuffer.buffer) != VK_SUCCESS)
    {
        return false;
    }

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(m_device, outBuffer.buffer, &memReqs);

    VkMemoryAllocateFlagsInfo allocFlagsInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO };
    if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
    {
        allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
    }

    VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
    allocInfo.pNext = (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) ? &allocFlagsInfo : nullptr;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = FindMemoryType(memReqs.memoryTypeBits, properties);

    if (vkAllocateMemory(m_device, &allocInfo, nullptr, &outBuffer.memory) != VK_SUCCESS)
    {
        return false;
    }

    vkBindBufferMemory(m_device, outBuffer.buffer, outBuffer.memory, 0);

    if (properties & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
    {
        vkMapMemory(m_device, outBuffer.memory, 0, memReqs.size, 0, &outBuffer.pMapped);
    }

    if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT)
    {
        VkBufferDeviceAddressInfo addrInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
        addrInfo.buffer = outBuffer.buffer;
        outBuffer.deviceAddress = vkGetBufferDeviceAddressKHR_fn(m_device, &addrInfo);
    }

    return true;
}

//=============================================
// @brief
//
//=============================================
void CVulkanRayTracer::DestroyBuffer(vk_buffer_t& buffer)
{
    if (buffer.pMapped != nullptr)
    {
        vkUnmapMemory(m_device, buffer.memory);
        buffer.pMapped = nullptr;
    }
    if (buffer.buffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(m_device, buffer.buffer, nullptr);
        buffer.buffer = VK_NULL_HANDLE;
    }
    if (buffer.memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(m_device, buffer.memory, nullptr);
        buffer.memory = VK_NULL_HANDLE;
    }
    buffer.deviceAddress = 0;
    buffer.size = 0;
}

//=============================================
// @brief
//
//=============================================
void CVulkanRayTracer::EnsureBuffer(vk_buffer_t& buf, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
{
    if (buf.buffer != VK_NULL_HANDLE && buf.size >= size)
    {
        return;
    }

    DestroyBuffer(buf);
    VkDeviceSize allocSize = size * 2;
    CreateBuffer(allocSize, usage, properties, buf);
}

//=============================================
// @brief
//
//=============================================
VkShaderModule CVulkanRayTracer::LoadSPIRV(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
#ifdef _WIN32
        char exePath[MAX_PATH];
        if (GetModuleFileNameA(nullptr, exePath, MAX_PATH))
        {
            char* lastSlash = strrchr(exePath, '\\');
            if (!lastSlash)
            {
                lastSlash = strrchr(exePath, '/');
            }
            if (lastSlash)
            {
                *(lastSlash + 1) = '\0';
                std::string fullPath = std::string(exePath) + filename;
                file.open(fullPath, std::ios::ate | std::ios::binary);
            }
        }
#endif
        if (!file.is_open())
        {
            std::cerr << "Error: Could not open SPIR-V file: " << filename << "\n";
            return VK_NULL_HANDLE;
        }
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<Uint32> buffer(fileSize / sizeof(Uint32));
    file.seekg(0);
    file.read((char*)buffer.data(), fileSize);
    file.close();

    VkShaderModuleCreateInfo createInfo{ VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    createInfo.codeSize = fileSize;
    createInfo.pCode = buffer.data();

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
    {
        return VK_NULL_HANDLE;
    }
    return shaderModule;
}

//=============================================
// @brief
//
//=============================================
bool CVulkanRayTracer::CreateComputePipeline(VkShaderModule shaderModule, VkDescriptorSetLayout& outDescLayout, VkPipelineLayout& outPipeLayout, VkPipeline& outPipeline)
{
    VkDescriptorSetLayoutBinding bindings[5] = {};
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    bindings[2].binding = 2;
    bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[2].descriptorCount = 1;
    bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    bindings[3].binding = 3;
    bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    bindings[3].descriptorCount = 1;
    bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    bindings[4].binding = 4;
    bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[4].descriptorCount = 256;
    bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layoutInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    layoutInfo.bindingCount = 5;
    layoutInfo.pBindings = bindings;

    if (vkCreateDescriptorSetLayout(m_device, &layoutInfo, nullptr, &outDescLayout) != VK_SUCCESS)
    {
        return false;
    }

    VkPushConstantRange pushConstant{};
    pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushConstant.offset = 0;
    pushConstant.size = sizeof(Uint32) * 2;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &outDescLayout;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstant;

    if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &outPipeLayout) != VK_SUCCESS)
    {
        return false;
    }

    VkComputePipelineCreateInfo pipelineInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
    pipelineInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineInfo.stage.module = shaderModule;
    pipelineInfo.stage.pName = "main";
    pipelineInfo.layout = outPipeLayout;

    return vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &outPipeline) == VK_SUCCESS;
}

//=============================================
// @brief
//
//=============================================
bool CVulkanRayTracer::Initialize()
{
    VkApplicationInfo appInfo{ VK_STRUCTURE_TYPE_APPLICATION_INFO };
    appInfo.pApplicationName = "MapCompilerRAD";
    appInfo.apiVersion = VK_API_VERSION_1_2;

    const char* instanceExts[] = {
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
    };

    VkInstanceCreateInfo instInfo{ VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    instInfo.pApplicationInfo = &appInfo;
    instInfo.enabledExtensionCount = 1;
    instInfo.ppEnabledExtensionNames = instanceExts;

    if (vkCreateInstance(&instInfo, nullptr, &m_instance) != VK_SUCCESS)
    {
        std::cerr << "Error: Failed to create Vulkan instance.\n";
        return false;
    }

    Uint32 devCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &devCount, nullptr);
    if (devCount == 0)
    {
        return false;
    }

    std::vector<VkPhysicalDevice> devices(devCount);
    vkEnumeratePhysicalDevices(m_instance, &devCount, devices.data());
    m_physicalDevice = devices[0];

    Uint32 qFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &qFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> qFamilies(qFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &qFamilyCount, qFamilies.data());

    for (Uint32 i = 0; i < qFamilyCount; i++)
    {
        if (qFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            m_queueFamilyIndex = i;
            break;
        }
    }

    float qPriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{ VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
    queueInfo.queueFamilyIndex = m_queueFamilyIndex;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &qPriority;

    const char* devExtensions[] = {
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_QUERY_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
    };

    VkPhysicalDeviceBufferDeviceAddressFeatures addrFeats{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES };
    addrFeats.bufferDeviceAddress = VK_TRUE;

    VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeats{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR };
    rayQueryFeats.rayQuery = VK_TRUE;
    rayQueryFeats.pNext = &addrFeats;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeats{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR };
    asFeats.accelerationStructure = VK_TRUE;
    asFeats.pNext = &rayQueryFeats;

    VkDeviceCreateInfo devInfo{ VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    devInfo.pNext = &asFeats;
    devInfo.queueCreateInfoCount = 1;
    devInfo.pQueueCreateInfos = &queueInfo;
    devInfo.enabledExtensionCount = 4;
    devInfo.ppEnabledExtensionNames = devExtensions;

    if (vkCreateDevice(m_physicalDevice, &devInfo, nullptr, &m_device) != VK_SUCCESS)
    {
        std::cerr << "Error: Failed to create Vulkan Device with Ray Query / AS features.\n";
        return false;
    }

    vkGetDeviceQueue(m_device, m_queueFamilyIndex, 0, &m_queue);

    vkGetBufferDeviceAddressKHR_fn = (PFN_vkGetBufferDeviceAddressKHR)vkGetDeviceProcAddr(m_device, "vkGetBufferDeviceAddressKHR");
    vkCreateAccelerationStructureKHR_fn = (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(m_device, "vkCreateAccelerationStructureKHR");
    vkDestroyAccelerationStructureKHR_fn = (PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(m_device, "vkDestroyAccelerationStructureKHR");
    vkGetAccelerationStructureBuildSizesKHR_fn = (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(m_device, "vkGetAccelerationStructureBuildSizesKHR");
    vkGetAccelerationStructureDeviceAddressKHR_fn = (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetDeviceProcAddr(m_device, "vkGetAccelerationStructureDeviceAddressKHR");
    vkCmdBuildAccelerationStructuresKHR_fn = (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(m_device, "vkCmdBuildAccelerationStructuresKHR");

    VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = m_queueFamilyIndex;
    vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_cmdPool);

    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 16 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 64 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1024 }
    };
    VkDescriptorPoolCreateInfo descPoolInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    descPoolInfo.maxSets = 32;
    descPoolInfo.poolSizeCount = 3;
    descPoolInfo.pPoolSizes = poolSizes;
    vkCreateDescriptorPool(m_device, &descPoolInfo, nullptr, &m_descriptorPool);

    VkSamplerCreateInfo samplerInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
    vkCreateSampler(m_device, &samplerInfo, nullptr, &m_textureSampler);

    VkShaderModule pvsMod = LoadSPIRV("vis.comp.spv");
    if (pvsMod != VK_NULL_HANDLE)
    {
        CreateComputePipeline(pvsMod, m_pvsDescLayout, m_pvsPipelineLayout, m_pvsPipeline);
        vkDestroyShaderModule(m_device, pvsMod, nullptr);
    }

    VkShaderModule occMod = LoadSPIRV("ray_occlude.comp.spv");
    if (occMod != VK_NULL_HANDLE)
    {
        CreateComputePipeline(occMod, m_occludeDescLayout, m_occludePipelineLayout, m_occludePipeline);
        vkDestroyShaderModule(m_device, occMod, nullptr);
    }

    VkShaderModule intMod = LoadSPIRV("ray_intersect.comp.spv");
    if (intMod != VK_NULL_HANDLE)
    {
        CreateComputePipeline(intMod, m_intersectDescLayout, m_intersectPipelineLayout, m_intersectPipeline);
        vkDestroyShaderModule(m_device, intMod, nullptr);
    }

    VkCommandBufferAllocateInfo cmdAlloc{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cmdAlloc.commandPool = m_cmdPool;
    cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandBufferCount = 1;
    vkAllocateCommandBuffers(m_device, &cmdAlloc, &m_cmdBuffer);

    VkDescriptorSetAllocateInfo occSetAlloc{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    occSetAlloc.descriptorPool = m_descriptorPool;
    occSetAlloc.descriptorSetCount = 1;
    occSetAlloc.pSetLayouts = &m_occludeDescLayout;
    vkAllocateDescriptorSets(m_device, &occSetAlloc, &m_occludeDescSet);

    VkDescriptorSetAllocateInfo intSetAlloc{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    intSetAlloc.descriptorPool = m_descriptorPool;
    intSetAlloc.descriptorSetCount = 1;
    intSetAlloc.pSetLayouts = &m_intersectDescLayout;
    vkAllocateDescriptorSets(m_device, &intSetAlloc, &m_intersectDescSet);

    return true;
}

//=============================================
// @brief
//
//=============================================
void CVulkanRayTracer::Shutdown()
{
    if (m_device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(m_device);

        for (auto& gi : m_gpuImages)
        {
            if (gi.view) 
                vkDestroyImageView(m_device, gi.view, nullptr);
            if (gi.image) 
                vkDestroyImage(m_device, gi.image, nullptr);
            if (gi.memory)
                vkFreeMemory(m_device, gi.memory, nullptr);
        }
        m_gpuImages.clear();

        if (m_textureSampler)
        {
            vkDestroySampler(m_device, m_textureSampler, nullptr);
            m_textureSampler = VK_NULL_HANDLE;
        }

        DestroyBuffer(m_primBuf);

        if (m_pvsPipeline)
            vkDestroyPipeline(m_device, m_pvsPipeline, nullptr);
        if (m_pvsPipelineLayout)
            vkDestroyPipelineLayout(m_device, m_pvsPipelineLayout, nullptr);
        if (m_pvsDescLayout) 
            vkDestroyDescriptorSetLayout(m_device, m_pvsDescLayout, nullptr);

        if (m_occludePipeline)
            vkDestroyPipeline(m_device, m_occludePipeline, nullptr);
        if (m_occludePipelineLayout)
            vkDestroyPipelineLayout(m_device, m_occludePipelineLayout, nullptr);
        if (m_occludeDescLayout)
            vkDestroyDescriptorSetLayout(m_device, m_occludeDescLayout, nullptr);

        if (m_intersectPipeline)
            vkDestroyPipeline(m_device, m_intersectPipeline, nullptr);
        if (m_intersectPipelineLayout) 
            vkDestroyPipelineLayout(m_device, m_intersectPipelineLayout, nullptr);
        if (m_intersectDescLayout) 
            vkDestroyDescriptorSetLayout(m_device, m_intersectDescLayout, nullptr);

        if (m_tlas.handle) 
            vkDestroyAccelerationStructureKHR_fn(m_device, m_tlas.handle, nullptr);
        DestroyBuffer(m_tlas.buffer);

        if (m_blas.handle)
            vkDestroyAccelerationStructureKHR_fn(m_device, m_blas.handle, nullptr);
        DestroyBuffer(m_blas.buffer);

        DestroyBuffer(m_rayBuf);
        DestroyBuffer(m_hitBuf);

        if (m_descriptorPool) 
            vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
        if (m_cmdPool) 
            vkDestroyCommandPool(m_device, m_cmdPool, nullptr);

        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

    if (m_instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
}

//=============================================
// @brief
//
//=============================================
bool CVulkanRayTracer::BuildSceneBVH(const std::vector<Float>& vertices, const std::vector<Uint32>& indices)
{
    if (vertices.empty() || indices.empty())
    {
        return false;
    }

    vk_buffer_t vBuf, iBuf;
    CreateBuffer(vertices.size() * sizeof(Float), VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, vBuf);
    CreateBuffer(indices.size() * sizeof(Uint32), VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, iBuf);

    memcpy(vBuf.pMapped, vertices.data(), vBuf.size);
    memcpy(iBuf.pMapped, indices.data(), iBuf.size);

    VkAccelerationStructureGeometryKHR geom{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
    geom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geom.flags = 0;
    geom.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    geom.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geom.geometry.triangles.vertexData.deviceAddress = vBuf.deviceAddress;
    geom.geometry.triangles.vertexStride = sizeof(Float) * 3;
    geom.geometry.triangles.maxVertex = (Uint32)(vertices.size() / 3);
    geom.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
    geom.geometry.triangles.indexData.deviceAddress = iBuf.deviceAddress;

    VkAccelerationStructureBuildGeometryInfoKHR blasBuildInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
    blasBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    blasBuildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    blasBuildInfo.geometryCount = 1;
    blasBuildInfo.pGeometries = &geom;

    Uint32 primitiveCount = (Uint32)(indices.size() / 3);
    VkAccelerationStructureBuildSizesInfoKHR blasSizes{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    vkGetAccelerationStructureBuildSizesKHR_fn(m_device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &blasBuildInfo, &primitiveCount, &blasSizes);

    CreateBuffer(blasSizes.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_blas.buffer);

    VkAccelerationStructureCreateInfoKHR asCreateInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
    asCreateInfo.buffer = m_blas.buffer.buffer;
    asCreateInfo.size = blasSizes.accelerationStructureSize;
    asCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    vkCreateAccelerationStructureKHR_fn(m_device, &asCreateInfo, nullptr, &m_blas.handle);

    vk_buffer_t scratchBuf;
    CreateBuffer(blasSizes.buildScratchSize, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuf);

    blasBuildInfo.dstAccelerationStructure = m_blas.handle;
    blasBuildInfo.scratchData.deviceAddress = scratchBuf.deviceAddress;

    VkCommandBufferAllocateInfo cmdAlloc{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cmdAlloc.commandPool = m_cmdPool;
    cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(m_device, &cmdAlloc, &cmd);

    VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
    rangeInfo.primitiveCount = primitiveCount;
    const VkAccelerationStructureBuildRangeInfoKHR* pRangeInfo = &rangeInfo;

    vkCmdBuildAccelerationStructuresKHR_fn(cmd, 1, &blasBuildInfo, &pRangeInfo);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(m_queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_queue);

    vkFreeCommandBuffers(m_device, m_cmdPool, 1, &cmd);
    DestroyBuffer(scratchBuf);

    VkAccelerationStructureDeviceAddressInfoKHR addrInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
    addrInfo.accelerationStructure = m_blas.handle;
    VkDeviceAddress blasAddress = vkGetAccelerationStructureDeviceAddressKHR_fn(m_device, &addrInfo);

    VkAccelerationStructureInstanceKHR instance{};
    instance.transform.matrix[0][0] = 1.0f;
    instance.transform.matrix[1][1] = 1.0f;
    instance.transform.matrix[2][2] = 1.0f;
    instance.mask = 0xFF;
    instance.accelerationStructureReference = blasAddress;

    vk_buffer_t instBuf;
    CreateBuffer(sizeof(VkAccelerationStructureInstanceKHR), VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, instBuf);

    memcpy(instBuf.pMapped, &instance, sizeof(instance));

    VkAccelerationStructureGeometryKHR tlasGeom{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
    tlasGeom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    tlasGeom.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    tlasGeom.geometry.instances.data.deviceAddress = instBuf.deviceAddress;

    VkAccelerationStructureBuildGeometryInfoKHR tlasBuildInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
    tlasBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    tlasBuildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    tlasBuildInfo.geometryCount = 1;
    tlasBuildInfo.pGeometries = &tlasGeom;

    Uint32 instanceCount = 1;
    VkAccelerationStructureBuildSizesInfoKHR tlasSizes{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    vkGetAccelerationStructureBuildSizesKHR_fn(m_device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &tlasBuildInfo, &instanceCount, &tlasSizes);

    CreateBuffer(tlasSizes.accelerationStructureSize, VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_tlas.buffer);

    asCreateInfo.buffer = m_tlas.buffer.buffer;
    asCreateInfo.size = tlasSizes.accelerationStructureSize;
    asCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    vkCreateAccelerationStructureKHR_fn(m_device, &asCreateInfo, nullptr, &m_tlas.handle);

    CreateBuffer(tlasSizes.buildScratchSize, VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuf);

    tlasBuildInfo.dstAccelerationStructure = m_tlas.handle;
    tlasBuildInfo.scratchData.deviceAddress = scratchBuf.deviceAddress;

    vkAllocateCommandBuffers(m_device, &cmdAlloc, &cmd);
    vkBeginCommandBuffer(cmd, &beginInfo);

    VkAccelerationStructureBuildRangeInfoKHR tlasRangeInfo{};
    tlasRangeInfo.primitiveCount = 1;
    const VkAccelerationStructureBuildRangeInfoKHR* pTlasRange = &tlasRangeInfo;

    vkCmdBuildAccelerationStructuresKHR_fn(cmd, 1, &tlasBuildInfo, &pTlasRange);
    vkEndCommandBuffer(cmd);

    vkQueueSubmit(m_queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_queue);

    vkFreeCommandBuffers(m_device, m_cmdPool, 1, &cmd);
    DestroyBuffer(scratchBuf);
    DestroyBuffer(instBuf);

    return true;
}

//=============================================
// @brief
//
//=============================================
void CVulkanRayTracer::UploadGpuTextures(const std::vector<dds_image_t>& images)
{
    for (auto& gi : m_gpuImages)
    {
        if (gi.view) 
            vkDestroyImageView(m_device, gi.view, nullptr);
        if (gi.image) 
            vkDestroyImage(m_device, gi.image, nullptr);
        if (gi.memory) 
            vkFreeMemory(m_device, gi.memory, nullptr);
    }
    m_gpuImages.clear();

    m_gpuImages.resize(256);

    dds_image_t dummy;
    dummy.width = 1;
    dummy.height = 1;
    dummy.rgba = { 255, 255, 255, 255 };

    for (size_t i = 0; i < 256; i++)
    {
        const dds_image_t& src = (i < images.size() && !images[i].rgba.empty() && images[i].width > 0 && images[i].height > 0) ? images[i] : dummy;

        VkImageCreateInfo imgInfo{ VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
        imgInfo.imageType = VK_IMAGE_TYPE_2D;
        imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imgInfo.extent.width = (Uint32)src.width;
        imgInfo.extent.height = (Uint32)src.height;
        imgInfo.extent.depth = 1;
        imgInfo.mipLevels = 1;
        imgInfo.arrayLayers = 1;
        imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imgInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        vkCreateImage(m_device, &imgInfo, nullptr, &m_gpuImages[i].image);

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(m_device, m_gpuImages[i].image, &memReqs);

        VkMemoryAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = FindMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkAllocateMemory(m_device, &allocInfo, nullptr, &m_gpuImages[i].memory);
        vkBindImageMemory(m_device, m_gpuImages[i].image, m_gpuImages[i].memory, 0);

        VkImageViewCreateInfo viewInfo{ VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
        viewInfo.image = m_gpuImages[i].image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        vkCreateImageView(m_device, &viewInfo, nullptr, &m_gpuImages[i].view);

        vk_buffer_t stageBuf;
        VkDeviceSize imgBytes = src.width * src.height * 4;
        CreateBuffer(imgBytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stageBuf);

        memcpy(stageBuf.pMapped, src.rgba.data(), imgBytes);

        VkCommandBufferAllocateInfo cmdAlloc{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        cmdAlloc.commandPool = m_cmdPool;
        cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAlloc.commandBufferCount = 1;
        VkCommandBuffer cmd;
        vkAllocateCommandBuffers(m_device, &cmdAlloc, &cmd);

        VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        vkBeginCommandBuffer(cmd, &beginInfo);

        VkImageMemoryBarrier barrier1{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
        barrier1.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier1.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier1.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier1.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier1.image = m_gpuImages[i].image;
        barrier1.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier1.subresourceRange.baseMipLevel = 0;
        barrier1.subresourceRange.levelCount = 1;
        barrier1.subresourceRange.baseArrayLayer = 0;
        barrier1.subresourceRange.layerCount = 1;
        barrier1.srcAccessMask = 0;
        barrier1.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier1);

        VkBufferImageCopy copyRegion{};
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.layerCount = 1;
        copyRegion.imageExtent.width = (Uint32)src.width;
        copyRegion.imageExtent.height = (Uint32)src.height;
        copyRegion.imageExtent.depth = 1;
        vkCmdCopyBufferToImage(cmd, stageBuf.buffer, m_gpuImages[i].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

        VkImageMemoryBarrier barrier2 = barrier1;
        barrier2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier2.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier2.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier2.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier2);

        vkEndCommandBuffer(cmd);

        VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &cmd;
        vkQueueSubmit(m_queue, 1, &submit, VK_NULL_HANDLE);
        vkQueueWaitIdle(m_queue);

        vkFreeCommandBuffers(m_device, m_cmdPool, 1, &cmd);
        DestroyBuffer(stageBuf);
    }
}

//=============================================
// @brief
//
//=============================================
void CVulkanRayTracer::UploadPrimData(const std::vector<gpu_prim_data_t>& prims)
{
    if (prims.empty()) 
        return;

    VkDeviceSize bSize = prims.size() * sizeof(gpu_prim_data_t);
    EnsureBuffer(m_primBuf, bSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    memcpy(m_primBuf.pMapped, prims.data(), bSize);
}

//=============================================
// @brief
//
//=============================================
bool CVulkanRayTracer::RunPVSCompute(const std::vector<gpu_leaf_sample_t>& leafs, Uint32 numVisLeafs, std::vector<byte>& outPvsMatrix, Uint32 rowBytes)
{
    if (!m_pvsPipeline)
    {
        return false;
    }

    Uint32 rowDwords = (rowBytes + 3) / 4;
    size_t totalDwords = (size_t)numVisLeafs * rowDwords;

    vk_buffer_t inLeafBuf, outPvsBuf;
    CreateBuffer(leafs.size() * sizeof(gpu_leaf_sample_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, inLeafBuf);
    CreateBuffer(totalDwords * sizeof(Uint32), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT, outPvsBuf);

    memcpy(inLeafBuf.pMapped, leafs.data(), inLeafBuf.size);
    memset(outPvsBuf.pMapped, 0, outPvsBuf.size);

    VkDescriptorSetAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocInfo.descriptorPool = m_descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &m_pvsDescLayout;

    VkDescriptorSet descSet;
    vkAllocateDescriptorSets(m_device, &allocInfo, &descSet);

    VkWriteDescriptorSetAccelerationStructureKHR asDesc{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
    asDesc.accelerationStructureCount = 1;
    asDesc.pAccelerationStructures = &m_tlas.handle;

    VkWriteDescriptorSet writes[3] = {};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].pNext = &asDesc;
    writes[0].dstSet = descSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

    VkDescriptorBufferInfo bInfo1{ inLeafBuf.buffer, 0, inLeafBuf.size };
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = descSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].pBufferInfo = &bInfo1;

    VkDescriptorBufferInfo bInfo2{ outPvsBuf.buffer, 0, outPvsBuf.size };
    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = descSet;
    writes[2].dstBinding = 2;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[2].pBufferInfo = &bInfo2;

    vkUpdateDescriptorSets(m_device, 3, writes, 0, nullptr);

    VkCommandBufferAllocateInfo cmdAlloc{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cmdAlloc.commandPool = m_cmdPool;
    cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(m_device, &cmdAlloc, &cmd);

    VkCommandBufferBeginInfo bBegin{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(cmd, &bBegin);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pvsPipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, m_pvsPipelineLayout, 0, 1, &descSet, 0, nullptr);

    Uint32 pushConstants[2] = { numVisLeafs, rowDwords };
    vkCmdPushConstants(cmd, m_pvsPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConstants), pushConstants);

    Uint32 groupX = (numVisLeafs + 15) / 16;
    Uint32 groupY = (numVisLeafs + 15) / 16;
    vkCmdDispatch(cmd, groupX, groupY, 1);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(m_queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_queue);

    VkMappedMemoryRange range{ VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
    range.memory = outPvsBuf.memory;
    range.offset = 0;
    range.size = VK_WHOLE_SIZE;
    vkInvalidateMappedMemoryRanges(m_device, 1, &range);

    outPvsMatrix.resize(numVisLeafs * rowBytes, 0);
    const byte* srcBytes = reinterpret_cast<const byte*>(outPvsBuf.pMapped);
    for (Uint32 i = 0; i < numVisLeafs; i++)
    {
        memcpy(&outPvsMatrix[i * rowBytes], srcBytes + (i * rowDwords * sizeof(Uint32)), rowBytes);
    }

    vkFreeCommandBuffers(m_device, m_cmdPool, 1, &cmd);
    DestroyBuffer(inLeafBuf);
    DestroyBuffer(outPvsBuf);

    return true;
}

//=============================================
// @brief
//
//=============================================
bool CVulkanRayTracer::TraceOcclusionBatch(const std::vector<gpu_ray_t>& rays, std::vector<Uint32>& outHits)
{
    if (!m_occludePipeline || rays.empty())
    {
        return false;
    }

    size_t count = rays.size();
    outHits.resize(count, 0);

    EnsureBuffer(m_rayBuf, count * sizeof(gpu_ray_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    EnsureBuffer(m_hitBuf, count * sizeof(Uint32), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

    memcpy(m_rayBuf.pMapped, rays.data(), count * sizeof(gpu_ray_t));

    VkWriteDescriptorSetAccelerationStructureKHR asDesc{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
    asDesc.accelerationStructureCount = 1;
    asDesc.pAccelerationStructures = &m_tlas.handle;

    VkDescriptorImageInfo imageInfos[256];
    for (size_t i = 0; i < 256; i++)
    {
        imageInfos[i].sampler = m_textureSampler;
        imageInfos[i].imageView = (i < m_gpuImages.size()) ? m_gpuImages[i].view : VK_NULL_HANDLE;
        imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    VkWriteDescriptorSet writes[5] = {};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].pNext = &asDesc;
    writes[0].dstSet = m_occludeDescSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

    VkDescriptorBufferInfo bInfo1{ m_rayBuf.buffer, 0, count * sizeof(gpu_ray_t) };
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_occludeDescSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].pBufferInfo = &bInfo1;

    VkDescriptorBufferInfo bInfo2{ m_hitBuf.buffer, 0, count * sizeof(Uint32) };
    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = m_occludeDescSet;
    writes[2].dstBinding = 2;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[2].pBufferInfo = &bInfo2;

    VkDescriptorBufferInfo bInfo3{ m_primBuf.buffer, 0, m_primBuf.size };
    writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet = m_occludeDescSet;
    writes[3].dstBinding = 3;
    writes[3].descriptorCount = 1;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[3].pBufferInfo = &bInfo3;

    writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[4].dstSet = m_occludeDescSet;
    writes[4].dstBinding = 4;
    writes[4].descriptorCount = 256;
    writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[4].pImageInfo = imageInfos;

    vkUpdateDescriptorSets(m_device, 5, writes, 0, nullptr);

    vkResetCommandBuffer(m_cmdBuffer, 0);

    VkCommandBufferBeginInfo bBegin{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(m_cmdBuffer, &bBegin);

    vkCmdBindPipeline(m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_occludePipeline);
    vkCmdBindDescriptorSets(m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_occludePipelineLayout, 0, 1, &m_occludeDescSet, 0, nullptr);

    Uint32 numRays = (Uint32)count;
    vkCmdPushConstants(m_cmdBuffer, m_occludePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Uint32), &numRays);

    Uint32 groupX = (numRays + 63) / 64;
    vkCmdDispatch(m_cmdBuffer, groupX, 1, 1);

    vkEndCommandBuffer(m_cmdBuffer);

    VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &m_cmdBuffer;
    vkQueueSubmit(m_queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_queue);

    VkMappedMemoryRange range{ VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
    range.memory = m_hitBuf.memory;
    range.offset = 0;
    range.size = VK_WHOLE_SIZE;
    vkInvalidateMappedMemoryRanges(m_device, 1, &range);

    memcpy(outHits.data(), m_hitBuf.pMapped, count * sizeof(Uint32));

    return true;
}

//=============================================
// @brief
//
//=============================================
const gpu_ray_hit_t* CVulkanRayTracer::TraceRayHitBatch(const std::vector<gpu_ray_t>& rays)
{
    if (!m_intersectPipeline || rays.empty())
    {
        return nullptr;
    }

    size_t count = rays.size();

    EnsureBuffer(m_rayBuf, count * sizeof(gpu_ray_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    EnsureBuffer(m_hitBuf, count * sizeof(gpu_ray_hit_t), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT);

    memcpy(m_rayBuf.pMapped, rays.data(), count * sizeof(gpu_ray_t));

    VkWriteDescriptorSetAccelerationStructureKHR asDesc{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR };
    asDesc.accelerationStructureCount = 1;
    asDesc.pAccelerationStructures = &m_tlas.handle;

    VkDescriptorImageInfo imageInfos[256];
    for (size_t i = 0; i < 256; i++)
    {
        imageInfos[i].sampler = m_textureSampler;
        imageInfos[i].imageView = (i < m_gpuImages.size()) ? m_gpuImages[i].view : VK_NULL_HANDLE;
        imageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

    VkWriteDescriptorSet writes[5] = {};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].pNext = &asDesc;
    writes[0].dstSet = m_intersectDescSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

    VkDescriptorBufferInfo bInfo1{ m_rayBuf.buffer, 0, count * sizeof(gpu_ray_t) };
    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = m_intersectDescSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[1].pBufferInfo = &bInfo1;

    VkDescriptorBufferInfo bInfo2{ m_hitBuf.buffer, 0, count * sizeof(gpu_ray_hit_t) };
    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = m_intersectDescSet;
    writes[2].dstBinding = 2;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[2].pBufferInfo = &bInfo2;

    VkDescriptorBufferInfo bInfo3{ m_primBuf.buffer, 0, m_primBuf.size };
    writes[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[3].dstSet = m_intersectDescSet;
    writes[3].dstBinding = 3;
    writes[3].descriptorCount = 1;
    writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    writes[3].pBufferInfo = &bInfo3;

    writes[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[4].dstSet = m_intersectDescSet;
    writes[4].dstBinding = 4;
    writes[4].descriptorCount = 256;
    writes[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[4].pImageInfo = imageInfos;

    vkUpdateDescriptorSets(m_device, 5, writes, 0, nullptr);

    vkResetCommandBuffer(m_cmdBuffer, 0);

    VkCommandBufferBeginInfo bBegin{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(m_cmdBuffer, &bBegin);

    vkCmdBindPipeline(m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_intersectPipeline);
    vkCmdBindDescriptorSets(m_cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_intersectPipelineLayout, 0, 1, &m_intersectDescSet, 0, nullptr);

    Uint32 numRays = (Uint32)count;
    vkCmdPushConstants(m_cmdBuffer, m_intersectPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(Uint32), &numRays);

    Uint32 groupX = (numRays + 63) / 64;
    vkCmdDispatch(m_cmdBuffer, groupX, 1, 1);

    vkEndCommandBuffer(m_cmdBuffer);

    VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &m_cmdBuffer;
    vkQueueSubmit(m_queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_queue);

    VkMappedMemoryRange range{ VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE };
    range.memory = m_hitBuf.memory;
    range.offset = 0;
    range.size = VK_WHOLE_SIZE;
    vkInvalidateMappedMemoryRanges(m_device, 1, &range);

    m_hostHits.resize(count);

    memcpy(m_hostHits.data(), m_hitBuf.pMapped, count * sizeof(gpu_ray_hit_t));

    return m_hostHits.data();
}