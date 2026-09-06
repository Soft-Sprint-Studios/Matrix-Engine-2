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
#include "rad.h"
#include "bsp.h"
#include "miniz.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <omp.h>

static constexpr Int32 FL_OCTREE_OCCLUDED = (1 << 31);
static constexpr Int32 FL_OCTREE_LEAF = (1 << 30);

static inline Int32 GridSampleIndex(Int32 x, Int32 y, Int32 z, const Int32 size[3])
{
    return (size[0] * size[1] * z) + (size[0] * y) + x;
}

Int32 CRadPipeline::BuildGridOctree(const Int32 mins[3], const Int32 size[3], Int32 depth, const Int32 gridSize[3], const std::vector<grid_sample_t>& samples, std::vector<grid_octree_node_t>& nodes, std::vector<grid_octree_leaf_t>& leaves, Int32& outOccludedCount)
{
    Int32 numOccluded = 0;
    Int32 numUnoccluded = 0;

    for (Int32 z = mins[2]; z < mins[2] + size[2]; z++)
    {
        for (Int32 y = mins[1]; y < mins[1] + size[1]; y++)
        {
            for (Int32 x = mins[0]; x < mins[0] + size[0]; x++)
            {
                Int32 idx = GridSampleIndex(x, y, z, gridSize);
                if (samples[idx].occluded)
                    numOccluded++;
                else
                    numUnoccluded++;
            }
        }
    }

    if (numUnoccluded == 0)
    {
        outOccludedCount += (size[0] * size[1] * size[2]);
        return FL_OCTREE_OCCLUDED;
    }

    if (size[0] <= 4 || size[1] <= 4 || size[2] <= 4 || depth >= 5 || numUnoccluded < 8)
    {
        grid_octree_leaf_t leaf;
        leaf.mins[0] = mins[0]; leaf.mins[1] = mins[1]; leaf.mins[2] = mins[2];
        leaf.size[0] = size[0]; leaf.size[1] = size[1]; leaf.size[2] = size[2];
        leaf.firstsample = 0;
        leaf.numsamples = size[0] * size[1] * size[2];

        Int32 leafIndex = (Int32)leaves.size();
        leaves.push_back(leaf);
        return (FL_OCTREE_LEAF | leafIndex);
    }

    Int32 divPoint[3] = {
        mins[0] + size[0] / 2,
        mins[1] + size[1] / 2,
        mins[2] + size[2] / 2
    };

    grid_octree_node_t node;
    node.divisionpoint[0] = divPoint[0];
    node.divisionpoint[1] = divPoint[1];
    node.divisionpoint[2] = divPoint[2];

    Int32 nodeIndex = (Int32)nodes.size();
    nodes.push_back(node);

    for (Int32 i = 0; i < 8; i++)
    {
        Int32 childMins[3], childSize[3];
        for (Int32 a = 0; a < 3; a++)
        {
            Int32 bit = (a == 0) ? 4 : ((a == 1) ? 2 : 1);
            if (i & bit)
            {
                childMins[a] = divPoint[a];
                childSize[a] = mins[a] + size[a] - divPoint[a];
            }
            else
            {
                childMins[a] = mins[a];
                childSize[a] = divPoint[a] - mins[a];
            }
        }

        Int32 childRef = BuildGridOctree(childMins, childSize, depth + 1, gridSize, samples, nodes, leaves, outOccludedCount);
        nodes[nodeIndex].children[i] = childRef;
    }

    return nodeIndex;
}

void CRadPipeline::BuildLightGrid(Int32 gridDistance)
{
    if (g_BSP.GetModelCount() == 0)
        return;

    std::cout << "Baking Light Grid...\n";

    const dmbspv1model_t& worldModel = g_BSP.GetFaceCount() > 0 ? g_BSP.GetFace(0), dmbspv1model_t() : dmbspv1model_t();
    Float worldMins[3] = { 999999.0f, 999999.0f, 999999.0f };
    Float worldMaxs[3] = { -999999.0f, -999999.0f, -999999.0f };

    size_t faceCount = g_BSP.GetFaceCount();
    for (size_t i = 0; i < faceCount; i++)
    {
        const auto& f = g_BSP.GetFace(i);
        for (Int32 e = 0; e < f.numedges; e++)
        {
            Int32 surfEdge = g_BSP.GetSurfEdge(f.firstedge + e);
            Uint32 vIdx = (surfEdge >= 0) ? g_BSP.GetEdge(surfEdge).vertexes[0] : g_BSP.GetEdge(-surfEdge).vertexes[1];
            const auto& v = g_BSP.GetVertex(vIdx);

            for (Int32 k = 0; k < 3; k++)
            {
                if (v.origin[k] < worldMins[k]) 
                    worldMins[k] = v.origin[k];
                if (v.origin[k] > worldMaxs[k]) 
                    worldMaxs[k] = v.origin[k];
            }
        }
    }

    Int32 gridDist[3] = { gridDistance, gridDistance, gridDistance };

    Int32 gridSize[3] = {
        std::max(1, (Int32)ceilf((worldMaxs[0] - worldMins[0]) / (Float)gridDist[0])),
        std::max(1, (Int32)ceilf((worldMaxs[1] - worldMins[1]) / (Float)gridDist[1])),
        std::max(1, (Int32)ceilf((worldMaxs[2] - worldMins[2]) / (Float)gridDist[2]))
    };

    size_t totalSamples = (size_t)gridSize[0] * gridSize[1] * gridSize[2];
    std::vector<grid_sample_t> samples(totalSamples);

    #pragma omp parallel for schedule(dynamic)
    for (Int32 z = 0; z < gridSize[2]; z++)
    {
        for (Int32 y = 0; y < gridSize[1]; y++)
        {
            for (Int32 x = 0; x < gridSize[0]; x++)
            {
                Int32 idx = GridSampleIndex(x, y, z, gridSize);
                grid_sample_t& s = samples[idx];
                memset(&s, 0, sizeof(s));
                memset(s.styles, 255, sizeof(s.styles));
                s.styles[0] = 0;

                s.worldPos[0] = worldMins[0] + x * gridDist[0];
                s.worldPos[1] = worldMins[1] + y * gridDist[1];
                s.worldPos[2] = worldMins[2] + z * gridDist[2];

                Float hitDist;
                Float upTest[3] = { s.worldPos[0], s.worldPos[1], s.worldPos[2] + 4096.0f };
                Float downTest[3] = { s.worldPos[0], s.worldPos[1], s.worldPos[2] - 4096.0f };

                bool hitUp = TraceOcclusion(s.worldPos, upTest, hitDist);
                bool hitDown = TraceOcclusion(s.worldPos, downTest, hitDist);

                if (hitUp && hitDown && hitDist < 2.0f)
                {
                    s.occluded = true;
                    continue;
                }

                Float styleDirect[64][3] = { 0.0f };
                Float styleDir[64][3] = { 0.0f };

                for (const auto& lt : m_lights)
                {
                    Int32 style = (lt.style >= 0 && lt.style < 64) ? lt.style : 0;

                    if (lt.type == LIGHT_POINT || lt.type == LIGHT_SPOT)
                    {
                        Float toLight[3] = { lt.origin[0] - s.worldPos[0], lt.origin[1] - s.worldPos[1], lt.origin[2] - s.worldPos[2] };
                        Float dist = sqrtf(toLight[0] * toLight[0] + toLight[1] * toLight[1] + toLight[2] * toLight[2]);
                        if (dist < 0.1f) 
                            continue;

                        Float dir[3] = { toLight[0] / dist, toLight[1] / dist, toLight[2] / dist };
                        Float spotFactor = 1.0f;
                        if (lt.type == LIGHT_SPOT)
                        {
                            Float spotDot = -(dir[0] * lt.normal[0] + dir[1] * lt.normal[1] + dir[2] * lt.normal[2]);
                            if (spotDot < lt.stopdot2) 
                                continue;

                            if (spotDot < lt.stopdot) 
                                spotFactor = (spotDot - lt.stopdot2) / (lt.stopdot - lt.stopdot2);
                        }

                        Float denom = (lt.falloff == 1) ? (dist * lt.fade) : (dist * dist * lt.fade);
                        Float atten = (1.0f / std::max(1.0f, denom)) * spotFactor;
                        Float maxColor = std::max({ lt.color[0], lt.color[1], lt.color[2] });
                        if (maxColor * atten < 0.05f)
                            continue;

                        if (!TraceOcclusion(s.worldPos, lt.origin, hitDist))
                        {
                            styleDirect[style][0] += lt.color[0] * atten;
                            styleDirect[style][1] += lt.color[1] * atten;
                            styleDirect[style][2] += lt.color[2] * atten;

                            styleDir[style][0] += dir[0];
                            styleDir[style][1] += dir[1];
                            styleDir[style][2] += dir[2];
                        }
                    }
                }

                Float maxLightPerStyle[64] = { 0.0f };
                for (Int32 st = 0; st < 64; st++)
                {
                    maxLightPerStyle[st] = std::max({ styleDirect[st][0], styleDirect[st][1], styleDirect[st][2] });
                }

                s.styles[0] = 0;
                for (Int32 slot = 1; slot < MBSPV1_MAX_LIGHTMAPS; slot++)
                {
                    Int32 bestStyle = -1;
                    Float bestVal = 0.1f;
                    for (Int32 st = 1; st < 64; st++)
                    {
                        if (maxLightPerStyle[st] > bestVal)
                        {
                            bestVal = maxLightPerStyle[st];
                            bestStyle = st;
                        }
                    }
                    if (bestStyle != -1)
                    {
                        s.styles[slot] = (byte)bestStyle;
                        maxLightPerStyle[bestStyle] = 0.0f;
                    }
                }

                for (Int32 slot = 0; slot < MBSPV1_MAX_LIGHTMAPS; slot++)
                {
                    if (s.styles[slot] == 255) 
                        continue;

                    Int32 st = s.styles[slot];

                    s.diffuse[slot][0] = styleDirect[st][0] * 0.7f;
                    s.diffuse[slot][1] = styleDirect[st][1] * 0.7f;
                    s.diffuse[slot][2] = styleDirect[st][2] * 0.7f;

                    s.ambient[slot][0] = styleDirect[st][0] * 0.3f;
                    s.ambient[slot][1] = styleDirect[st][1] * 0.3f;
                    s.ambient[slot][2] = styleDirect[st][2] * 0.3f;

                    s.dominantDir[slot][0] = styleDir[st][0];
                    s.dominantDir[slot][1] = styleDir[st][1];
                    s.dominantDir[slot][2] = styleDir[st][2];
                }
                const Int32 numProbeRays = 16;
                Float bounceRadiance[3] = { 0.0f, 0.0f, 0.0f };
                for (Int32 r = 0; r < numProbeRays; r++)
                {
                    Float theta = 2.0f * M_PI * ((Float)r / (Float)numProbeRays);
                    Float phi = acosf(1.0f - 2.0f * ((Float)r + 0.5f) / (Float)numProbeRays);

                    Float probeDir[3] = {
                        sinf(phi) * cosf(theta),
                        sinf(phi) * sinf(theta),
                        cosf(phi)
                    };

                    ray_hit_t hit;
                    if (TraceRayHit(s.worldPos, probeDir, 2048.0f, hit))
                    {
                        Int32 hitFace = m_primToFaceMap[hit.primID];
                        if (hitFace >= 0 && hitFace < (Int32)m_faceInfos.size())
                        {
                            Float albedo[3];
                            SampleHitAlbedo(hit.primID, hit.u, hit.v, albedo);

                            bounceRadiance[0] += (m_faceInfos[hitFace].avgRadiance[0] * albedo[0] + m_faceInfos[hitFace].emissive[0]);
                            bounceRadiance[1] += (m_faceInfos[hitFace].avgRadiance[1] * albedo[1] + m_faceInfos[hitFace].emissive[1]);
                            bounceRadiance[2] += (m_faceInfos[hitFace].avgRadiance[2] * albedo[2] + m_faceInfos[hitFace].emissive[2]);
                        }
                    }
                }

                s.ambient[0][0] += (bounceRadiance[0] / (Float)numProbeRays) * M_PI;
                s.ambient[0][1] += (bounceRadiance[1] / (Float)numProbeRays) * M_PI;
                s.ambient[0][2] += (bounceRadiance[2] / (Float)numProbeRays) * M_PI;
            }
        }
    }

    std::vector<grid_octree_node_t> octreeNodes;
    std::vector<grid_octree_leaf_t> octreeLeaves;
    Int32 rootMins[3] = { 0, 0, 0 };
    Int32 occludedCount = 0;

    Int32 rootNodeIdx = BuildGridOctree(rootMins, gridSize, 0, gridSize, samples, octreeNodes, octreeLeaves, occludedCount);

    Int32 rawDataSize = 0;
    std::vector<dmbspv1lightgridsample_t> bspSamples;

    for (auto& leaf : octreeLeaves)
    {
        leaf.firstsample = (Int32)bspSamples.size();
        for (Int32 z = leaf.mins[2]; z < leaf.mins[2] + leaf.size[2]; z++)
        {
            for (Int32 y = leaf.mins[1]; y < leaf.mins[1] + leaf.size[1]; y++)
            {
                for (Int32 x = leaf.mins[0]; x < leaf.mins[0] + leaf.size[0]; x++)
                {
                    Int32 idx = GridSampleIndex(x, y, z, gridSize);
                    grid_sample_t& s = samples[idx];

                    dmbspv1lightgridsample_t ds;
                    memcpy(ds.styles, s.styles, sizeof(ds.styles));

                    if (s.occluded)
                    {
                        ds.rawsampleoffset = -1;
                    }
                    else
                    {
                        Int32 styleCount = 0;
                        for (Int32 slot = 0; slot < MBSPV1_MAX_LIGHTMAPS; slot++)
                        {
                            if (s.styles[slot] != 255) 
                                styleCount++;
                        }

                        ds.rawsampleoffset = rawDataSize;
                        s.rawDataOffset = rawDataSize;
                        rawDataSize += styleCount;
                    }

                    bspSamples.push_back(ds);
                }
            }
        }
        leaf.numsamples = (Int32)bspSamples.size() - leaf.firstsample;
    }

    std::vector<byte> rawAmbient(rawDataSize * sizeof(Float) * 3, 0);
    std::vector<byte> rawDiffuse(rawDataSize * sizeof(Float) * 3, 0);
    std::vector<byte> rawVectors(rawDataSize * 3, 128);
    for (size_t vi = 2; vi < (size_t)(rawDataSize * 3); vi += 3)
    {
        rawVectors[vi] = 255;
    }

    for (const auto& s : samples)
    {
        if (s.occluded || s.rawDataOffset < 0)
            continue;

        Int32 sampleIdx = s.rawDataOffset;
        for (Int32 slot = 0; slot < MBSPV1_MAX_LIGHTMAPS; slot++)
        {
            if (s.styles[slot] == 255)
                continue;

            Float* pAmb = reinterpret_cast<Float*>(&rawAmbient[sampleIdx * sizeof(Float) * 3]);
            Float* pDiff = reinterpret_cast<Float*>(&rawDiffuse[sampleIdx * sizeof(Float) * 3]);

            auto ColorToHDR = [](Float val) -> Float
                {
                    if (val <= 0.0f) return 0.0f;
                    return powf(val / 128.0f, 0.55f) * 2.0f;
                };

            pAmb[0] = ColorToHDR(s.ambient[slot][0]);
            pAmb[1] = ColorToHDR(s.ambient[slot][1]);
            pAmb[2] = ColorToHDR(s.ambient[slot][2]);

            pDiff[0] = ColorToHDR(s.diffuse[slot][0]);
            pDiff[1] = ColorToHDR(s.diffuse[slot][1]);
            pDiff[2] = ColorToHDR(s.diffuse[slot][2]);

            Float dLen = sqrtf(s.dominantDir[slot][0] * s.dominantDir[slot][0] + s.dominantDir[slot][1] * s.dominantDir[slot][1] + s.dominantDir[slot][2] * s.dominantDir[slot][2]);
            Float normD[3] = { 0.0f, 0.0f, 1.0f };
            if (dLen > 0.001f)
            {
                normD[0] = s.dominantDir[slot][0] / dLen;
                normD[1] = s.dominantDir[slot][1] / dLen;
                normD[2] = s.dominantDir[slot][2] / dLen;
            }

            rawVectors[sampleIdx * 3 + 0] = (byte)std::clamp((Int32)((normD[0] * 0.5f + 0.5f) * 255.0f), 0, 255);
            rawVectors[sampleIdx * 3 + 1] = (byte)std::clamp((Int32)((normD[1] * 0.5f + 0.5f) * 255.0f), 0, 255);
            rawVectors[sampleIdx * 3 + 2] = (byte)std::clamp((Int32)((normD[2] * 0.5f + 0.5f) * 255.0f), 0, 255);

            sampleIdx++;
        }
    }

    auto CompressLayer = [](const std::vector<byte>& src, std::vector<byte>& dst, Int32& compSize)
        {
            if (src.empty())
            {
                compSize = 0;
                return;
            }
            mz_ulong maxDst = mz_compressBound((mz_ulong)src.size());
            dst.resize(maxDst);
            int status = mz_compress2(dst.data(), &maxDst, src.data(), (mz_ulong)src.size(), MZ_DEFAULT_LEVEL);
            if (status == MZ_OK)
            {
                dst.resize(maxDst);
                compSize = (Int32)maxDst;
            }
            else
            {
                dst = src;
                compSize = (Int32)src.size();
            }
        };

    std::vector<byte> compAmb, compDiff, compVec;
    Int32 compAmbSize = 0, compDiffSize = 0, compVecSize = 0;

    #pragma omp parallel sections
    {
        #pragma omp section
        CompressLayer(rawAmbient, compAmb, compAmbSize);
        #pragma omp section
        CompressLayer(rawDiffuse, compDiff, compDiffSize);
        #pragma omp section
        CompressLayer(rawVectors, compVec, compVecSize);
    }

    dmbspv1lightgridlumpheader_t hdr;
    for (Int32 k = 0; k < 3; k++)
    {
        hdr.grid_distance[k] = gridDist[k];
        hdr.grid_size[k] = gridSize[k];
        hdr.grid_mins[k] = worldMins[k];
    }

    hdr.rootnodeindex = rootNodeIdx;
    hdr.rawsampledatasize = rawDataSize;

    Int32 dataOffset = (Int32)sizeof(dmbspv1lightgridlumpheader_t);

    hdr.nodesoffset = dataOffset;
    hdr.numnodes = (Int32)octreeNodes.size();
    dataOffset += (Int32)(octreeNodes.size() * sizeof(dmbspv1lightgridnode_t));

    hdr.sampleoffset = dataOffset;
    hdr.numsamples = (Int32)bspSamples.size();
    dataOffset += (Int32)(bspSamples.size() * sizeof(dmbspv1lightgridsample_t));

    hdr.leafsoffset = dataOffset;
    hdr.numleafs = (Int32)octreeLeaves.size();
    dataOffset += (Int32)(octreeLeaves.size() * sizeof(dmbspv1lightgridleaf_t));

    hdr.ambientdataoffset = dataOffset;
    hdr.ambientcompressedsize = compAmbSize;
    hdr.ambientcompressionlevel = MZ_DEFAULT_LEVEL;
    hdr.ambientcompressiontype = 1;
    dataOffset += compAmbSize;

    hdr.diffusedataoffset = dataOffset;
    hdr.diffusecompressedsize = compDiffSize;
    hdr.diffusecompressionlevel = MZ_DEFAULT_LEVEL;
    hdr.diffusecompressiontype = 1;
    dataOffset += compDiffSize;

    hdr.vectorsdataoffset = dataOffset;
    hdr.vectorscompressedsize = compVecSize;
    hdr.vectorscompressionlevel = MZ_DEFAULT_LEVEL;
    hdr.vectorscompressiontype = 1;
    dataOffset += compVecSize;

    hdr.totalsize = dataOffset;

    std::vector<byte> finalGridLump(hdr.totalsize);
    byte* pOut = finalGridLump.data();

    memcpy(pOut, &hdr, sizeof(hdr));

    std::vector<dmbspv1lightgridnode_t> bspNodes(octreeNodes.size());
    for (size_t i = 0; i < octreeNodes.size(); i++)
    {
        memcpy(bspNodes[i].divisionpoint, octreeNodes[i].divisionpoint, sizeof(bspNodes[i].divisionpoint));
        memcpy(bspNodes[i].children, octreeNodes[i].children, sizeof(bspNodes[i].children));
    }
    memcpy(pOut + hdr.nodesoffset, bspNodes.data(), bspNodes.size() * sizeof(dmbspv1lightgridnode_t));
    memcpy(pOut + hdr.sampleoffset, bspSamples.data(), bspSamples.size() * sizeof(dmbspv1lightgridsample_t));

    std::vector<dmbspv1lightgridleaf_t> bspLeaves(octreeLeaves.size());
    for (size_t i = 0; i < octreeLeaves.size(); i++)
    {
        memcpy(bspLeaves[i].mins, octreeLeaves[i].mins, sizeof(bspLeaves[i].mins));
        memcpy(bspLeaves[i].size, octreeLeaves[i].size, sizeof(bspLeaves[i].size));
        bspLeaves[i].firstsample = octreeLeaves[i].firstsample;
        bspLeaves[i].numsamples = octreeLeaves[i].numsamples;
    }
    memcpy(pOut + hdr.leafsoffset, bspLeaves.data(), bspLeaves.size() * sizeof(dmbspv1lightgridleaf_t));

    if (!compAmb.empty())
        memcpy(pOut + hdr.ambientdataoffset, compAmb.data(), compAmbSize);
    if (!compDiff.empty())
        memcpy(pOut + hdr.diffusedataoffset, compDiff.data(), compDiffSize);
    if (!compVec.empty())
        memcpy(pOut + hdr.vectorsdataoffset, compVec.data(), compVecSize);

    g_BSP.SetLightGridData(finalGridLump);
}