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
#include <embree4/rtcore.h>

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

struct ray_hit_t
{
    bool hit;
    Float dist;
    Float normal[3];
    Uint32 geomID;
    Uint32 primID;
    Float u;
    Float v;
};

class CRadPipeline
{
public:
    CRadPipeline();
    ~CRadPipeline();

    bool InitializeEmbree();
    void BuildSceneGeometry(const map_data_t& mapData, const map_disp_data_t& dispData, const Char* baseDir);
    void ParseLights(map_data_t& mapData, const std::string& daystage = "");
    void LoadTexlights(const Char* baseDir);
    bool TraceOcclusion(const Float start[3], const Float end[3], Float& outDist) const;
    bool TraceRayHit(const Float start[3], const Float dir[3], Float maxDist, ray_hit_t& outHit) const;
    void SampleHitAlbedo(Uint32 primID, Float u, Float v, Float outAlbedo[3]) const;
    void BakeLightmaps(std::vector<lightmap_face_t>& faceLightmaps, const Char* baseDir, Int32 numBounces = 8, Int32 raysPerLuxel = 32);
    void BakeVertexLights(map_data_t& mapData, const Char* baseDir);
    void BuildLightGrid(Int32 gridDistance = 32);
    void Shutdown();

private:
    void AddSunLight(const map_entity_t& ent, Int32 style);
    void AddPointLight(const map_entity_t& ent, Int32 style);
    void AddSpotLight(const map_entity_t& ent, Int32 style);
    static void AlphaTestFilterCallback(const struct RTCFilterFunctionNArguments* args);

    RTCDevice m_device;
    RTCScene m_scene;
    std::vector<rad_light_t> m_lights;

    struct face_info_t
    {
        Float reflectScale;
        Float avgRadiance[3];
        Float emissive[3];
        Float minLight;
        bool hasAlphaTest;
        const dds_image_t* diffuseImage;
        bool ignoreNight;
    };
    std::vector<face_info_t> m_faceInfos;
    std::vector<Int32> m_primToFaceMap;

    struct scene_prim_t
    {
        Int32 faceIndex;
        Float uv[3][2];
    };
    std::vector<scene_prim_t> m_scenePrims;
    std::unordered_map<std::string, material_t> m_materials;
    std::unordered_map<std::string, std::array<Float, 3>> m_texlights;

    struct grid_sample_t
    {
        bool occluded;
        Float worldPos[3];
        byte styles[MBSPV1_MAX_LIGHTMAPS];
        Float ambient[MBSPV1_MAX_LIGHTMAPS][3];
        Float diffuse[MBSPV1_MAX_LIGHTMAPS][3];
        Float dominantDir[MBSPV1_MAX_LIGHTMAPS][3];
        Int32 rawDataOffset;
    };

    struct grid_octree_node_t
    {
        Int32 divisionpoint[3];
        Int32 children[8];
    };

    struct grid_octree_leaf_t
    {
        Int32 mins[3];
        Int32 size[3];
        Int32 firstsample;
        Int32 numsamples;
    };

    Int32 BuildGridOctree(const Int32 mins[3], const Int32 size[3], Int32 depth, const Int32 gridSize[3], const std::vector<grid_sample_t>& samples, std::vector<grid_octree_node_t>& nodes, std::vector<grid_octree_leaf_t>& leaves, Int32& outOccludedCount);

};

#endif // RAD_H