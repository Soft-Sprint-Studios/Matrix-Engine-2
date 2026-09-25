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
#ifndef RAD_H
#define RAD_H

#include "datatypes.h"
#include "mapparser.h"
#include "lightmap.h"
#include "pmf.h"
#include <vector>
#include <array>
#include <unordered_map>
#include "vk_rad.h"

enum rad_lighttype_t
{
    LIGHT_NONE = 0,
    LIGHT_POINT = 1,
    LIGHT_SPOT = 2,
    LIGHT_SUN = 3
};

struct rad_light_t
{
    Int32 type;
    Int32 style;
    Int32 falloff;
    Float origin[3];
    Float color[3];
    Float normal[3];
    Float stopdot;
    Float stopdot2;
    Float fade;
};

class CRadPipeline
{
public:
    CRadPipeline();
    ~CRadPipeline();

    bool InitializeVulkan();
    bool ComputePVSGPU(size_t numVisLeafs, const std::vector<gpu_leaf_sample_t>& leafs, std::vector<byte>& outPvsMatrix, size_t rowBytes) const;
    bool TraceOcclusionBatch(const std::vector<gpu_ray_t>& rays, std::vector<Uint32>& outHits);
    const gpu_ray_hit_t* TraceRayHitBatch(const std::vector<gpu_ray_t>& rays);
    void BuildSceneGeometry(const map_data_t& mapData, const map_disp_data_t& dispData, const Char* baseDir);
    void ParseLights(map_data_t& mapData, const std::string& daystage = "");
    void BakeLightmaps(std::vector<lightmap_face_t>& faceLightmaps, const Char* baseDir, Int32 numBounces, Int32 raysPerLuxel);
    void BakeVertexLights(map_data_t& mapData, const Char* baseDir, Int32 raysPerLuxel);
    void BuildLightGrid(Int32 gridDistance, Int32 raysPerLuxel);
    void Shutdown();

private:
    void AddSunLight(const map_entity_t& ent, Int32 style);
    void AddPointLight(const map_entity_t& ent, Int32 style);
    void AddSpotLight(const map_entity_t& ent, Int32 style);
    CVulkanRayTracer m_vk;

    std::vector<rad_light_t> m_lights;

    struct face_info_t
    {
        Float emissive[3];
        Float minLight;
        bool hasAlphaTest;
        const dds_image_t* diffuseImage;
    };

    void GetHitSurfaceRadiance(Int32 hitFace, const Float hitPos[3], Float outRad[3]) const;
    struct baked_luxel_t
    {
        Float r, g, b;
    };
    std::vector<std::vector<baked_luxel_t>> m_bakedLuxels;
    std::vector<lightmap_face_t> m_bakedFaceLightmaps;
    std::vector<face_info_t> m_faceInfos;
};

#endif