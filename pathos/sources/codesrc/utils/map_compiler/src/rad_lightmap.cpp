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
#include "rad_phong.h"
#include "bsp.h"
#include "miniz.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <omp.h>

struct fast_trig_t
{
    static constexpr Int32 TABLE_SIZE = 4096;
    Float sinTable[TABLE_SIZE];
    Float cosTable[TABLE_SIZE];

    fast_trig_t()
    {
        for (Int32 i = 0; i < TABLE_SIZE; i++)
        {
            Float angle = (Float)i * (2.0f * M_PI / (Float)TABLE_SIZE);
            sinTable[i] = sinf(angle);
            cosTable[i] = cosf(angle);
        }
    }

    inline void SinCosFrac(Float u, Float& outSin, Float& outCos) const
    {
        Int32 idx = (Int32)(u * (Float)TABLE_SIZE) & (TABLE_SIZE - 1);
        outSin = sinTable[idx];
        outCos = cosTable[idx];
    }
};

static const fast_trig_t g_fastTrig;

void CRadPipeline::BakeLightmaps(std::vector<lightmap_face_t>& faceLightmaps, const Char* baseDir, Int32 numBounces, Int32 raysPerLuxel)
{
    std::cout << "Baking lightmaps...\n";

    SmoothFaceNormals(faceLightmaps);

    struct luxel_radiance_t
    {
        Float direct[MBSPV1_MAX_LIGHTMAPS][3];
        Float bounce[MBSPV1_MAX_LIGHTMAPS][3];
        Float ambient[3];
        Float dominantDir[MBSPV1_MAX_LIGHTMAPS][3];
        Float sunDirect[3];
    };

    std::vector<std::vector<luxel_radiance_t>> faceLuxels(faceLightmaps.size());

    struct luxel_ref_t
    {
        int f;
        int i;
    };

    std::vector<luxel_ref_t> activeLuxels;

    for (int f = 0; f < (int)faceLightmaps.size(); f++)
    {
        const auto& lm = faceLightmaps[f];
        if (g_BSP.GetTexinfo(lm.texinfoIndex).flags & 1)
        {
            continue;
        }

        faceLuxels[f].resize(lm.totalLuxels);

        for (Int32 i = 0; i < lm.totalLuxels; i++)
        {
            activeLuxels.push_back({ f, i });
            auto& lux = faceLuxels[f][i];
            memset(&lux, 0, sizeof(lux));

            if (lm.bspFaceIndex >= 0 && lm.bspFaceIndex < (Int32)m_faceInfos.size())
            {
                lux.direct[0][0] += m_faceInfos[lm.bspFaceIndex].emissive[0];
                lux.direct[0][1] += m_faceInfos[lm.bspFaceIndex].emissive[1];
                lux.direct[0][2] += m_faceInfos[lm.bspFaceIndex].emissive[2];
            }
        }
    }

    size_t numActive = activeLuxels.size();

    struct direct_job_t
    {
        int f;
        int i;
        bool isSun;
        Float color[3];
        Float dir[3];
        Int32 style;
        Float NdotL;
    };

    int maxThreads = omp_get_max_threads();
    std::vector<std::vector<gpu_ray_t>> threadRays(maxThreads);
    std::vector<std::vector<direct_job_t>> threadJobs(maxThreads);

    #pragma omp parallel
    {
        int tid = omp_get_thread_num();

        #pragma omp for schedule(static)
        for (int idx = 0; idx < (int)numActive; idx++)
        {
            int f = activeLuxels[idx].f;
            int i = activeLuxels[idx].i;
            const auto& coord = faceLightmaps[f].sampleCoords[i];

            for (const auto& lt : m_lights)
            {
                if (lt.type == LIGHT_SUN)
                {
                    Float NdotL = -(coord.normal[0] * lt.normal[0] + coord.normal[1] * lt.normal[1] + coord.normal[2] * lt.normal[2]);
                    if (NdotL <= 0.001f)
                    {
                        continue;
                    }

                    gpu_ray_t gray;
                    gray.origin[0] = coord.worldPos[0];
                    gray.origin[1] = coord.worldPos[1];
                    gray.origin[2] = coord.worldPos[2];
                    gray.tMin = 0.05f;
                    gray.dir[0] = -lt.normal[0];
                    gray.dir[1] = -lt.normal[1];
                    gray.dir[2] = -lt.normal[2];
                    gray.tMax = 32768.0f;

                    threadRays[tid].push_back(gray);
                    threadJobs[tid].push_back({ f, i, true, { lt.color[0] * NdotL, lt.color[1] * NdotL, lt.color[2] * NdotL }, { 0.0f, 0.0f, 0.0f }, 0, NdotL });
                }
                else if (lt.type == LIGHT_POINT || lt.type == LIGHT_SPOT)
                {
                    Float toLight[3] = { lt.origin[0] - coord.worldPos[0], lt.origin[1] - coord.worldPos[1], lt.origin[2] - coord.worldPos[2] };
                    Float dist = sqrtf(toLight[0] * toLight[0] + toLight[1] * toLight[1] + toLight[2] * toLight[2]);
                    if (dist < 0.1f)
                    {
                        continue;
                    }

                    Float dir[3] = { toLight[0] / dist, toLight[1] / dist, toLight[2] / dist };
                    Float NdotL = coord.normal[0] * dir[0] + coord.normal[1] * dir[1] + coord.normal[2] * dir[2];
                    if (NdotL <= 0.001f)
                    {
                        continue;
                    }

                    Float spotFactor = 1.0f;
                    if (lt.type == LIGHT_SPOT)
                    {
                        Float spotDot = -(dir[0] * lt.normal[0] + dir[1] * lt.normal[1] + dir[2] * lt.normal[2]);
                        if (spotDot < lt.stopdot2)
                        {
                            continue;
                        }

                        if (spotDot < lt.stopdot)
                        {
                            spotFactor = (spotDot - lt.stopdot2) / (lt.stopdot - lt.stopdot2);
                        }
                    }

                    Float denom = (lt.falloff == 1) ? (dist * lt.fade) : (dist * dist * lt.fade);
                    Float atten = (1.0f / std::max(1.0f, denom)) * spotFactor * NdotL;
                    Float maxColor = std::max({ lt.color[0], lt.color[1], lt.color[2] });
                    if (maxColor * atten < 0.05f)
                    {
                        continue;
                    }

                    gpu_ray_t gray;
                    gray.origin[0] = coord.worldPos[0];
                    gray.origin[1] = coord.worldPos[1];
                    gray.origin[2] = coord.worldPos[2];
                    gray.tMin = 0.05f;
                    gray.dir[0] = dir[0];
                    gray.dir[1] = dir[1];
                    gray.dir[2] = dir[2];
                    gray.tMax = dist - 0.05f;

                    threadRays[tid].push_back(gray);
                    threadJobs[tid].push_back({ f, i, false, { lt.color[0] * atten, lt.color[1] * atten, lt.color[2] * atten }, { dir[0], dir[1], dir[2] }, lt.style, NdotL });
                }
            }
        }
    }

    std::vector<gpu_ray_t> gpuDirectRays;
    std::vector<direct_job_t> directJobs;
    for (int t = 0; t < maxThreads; t++)
    {
        gpuDirectRays.insert(gpuDirectRays.end(), threadRays[t].begin(), threadRays[t].end());
        directJobs.insert(directJobs.end(), threadJobs[t].begin(), threadJobs[t].end());
    }

    std::vector<Uint32> directHits;
    TraceOcclusionBatch(gpuDirectRays, directHits);

    for (size_t k = 0; k < directHits.size(); k++)
    {
        if (directHits[k] == 0)
        {
            const auto& job = directJobs[k];
            auto& lux = faceLuxels[job.f][job.i];

            if (job.isSun)
            {
                lux.sunDirect[0] += job.color[0];
                lux.sunDirect[1] += job.color[1];
                lux.sunDirect[2] += job.color[2];
            }
            else
            {
                dmbspv1face_t& bspFace = g_BSP.GetFace(faceLightmaps[job.f].bspFaceIndex);
                Int32 styleSlot = -1;
                for (Int32 s = 0; s < MBSPV1_MAX_LIGHTMAPS; s++)
                {
                    if (bspFace.lmstyles[s] == job.style)
                    {
                        styleSlot = s;
                        break;
                    }
                    if (bspFace.lmstyles[s] == 255)
                    {
                        bspFace.lmstyles[s] = job.style;
                        styleSlot = s;
                        break;
                    }
                }

                if (styleSlot != -1)
                {
                    lux.direct[styleSlot][0] += job.color[0];
                    lux.direct[styleSlot][1] += job.color[1];
                    lux.direct[styleSlot][2] += job.color[2];

                    lux.dominantDir[styleSlot][0] += job.dir[0] * job.NdotL;
                    lux.dominantDir[styleSlot][1] += job.dir[1] * job.NdotL;
                    lux.dominantDir[styleSlot][2] += job.dir[2] * job.NdotL;
                }
            }
        }
    }

    for (size_t f = 0; f < faceLightmaps.size(); f++)
    {
        const auto& lm = faceLightmaps[f];
        if (!faceLuxels[f].empty() && lm.totalLuxels > 0 && lm.bspFaceIndex >= 0 && lm.bspFaceIndex < (Int32)m_faceInfos.size())
        {
            Float sumRad[3] = { 0.0f, 0.0f, 0.0f };
            for (Int32 i = 0; i < lm.totalLuxels; i++)
            {
                sumRad[0] += faceLuxels[f][i].direct[0][0] + faceLuxels[f][i].sunDirect[0];
                sumRad[1] += faceLuxels[f][i].direct[0][1] + faceLuxels[f][i].sunDirect[1];
                sumRad[2] += faceLuxels[f][i].direct[0][2] + faceLuxels[f][i].sunDirect[2];
            }
            m_faceInfos[lm.bspFaceIndex].avgRadiance[0] = sumRad[0] / (Float)lm.totalLuxels;
            m_faceInfos[lm.bspFaceIndex].avgRadiance[1] = sumRad[1] / (Float)lm.totalLuxels;
            m_faceInfos[lm.bspFaceIndex].avgRadiance[2] = sumRad[2] / (Float)lm.totalLuxels;
        }
    }

    const size_t CHUNK_LUXELS = 32768;
    std::vector<gpu_ray_t> gpuBounceRays;

    for (Int32 bounce = 0; bounce < numBounces; bounce++)
    {
        std::vector<std::vector<std::array<Float, 3>>> stepBounce(faceLightmaps.size());
        for (int f = 0; f < (int)faceLightmaps.size(); f++)
        {
            if (!(g_BSP.GetTexinfo(faceLightmaps[f].texinfoIndex).flags & 1))
            {
                stepBounce[f].assign(faceLightmaps[f].totalLuxels, { 0.0f, 0.0f, 0.0f });
            }
        }

        for (size_t chunkStart = 0; chunkStart < numActive; chunkStart += CHUNK_LUXELS)
        {
            size_t chunkSize = std::min(CHUNK_LUXELS, numActive - chunkStart);
            gpuBounceRays.resize(chunkSize * (size_t)raysPerLuxel);

            #pragma omp parallel for schedule(static)
            for (int cIdx = 0; cIdx < (int)chunkSize; cIdx++)
            {
                int idx = (int)(chunkStart + cIdx);
                int f = activeLuxels[idx].f;
                int i = activeLuxels[idx].i;
                const auto& coord = faceLightmaps[f].sampleCoords[i];

                Float tangent[3] = { 1.0f, 0.0f, 0.0f };
                if (fabsf(coord.normal[0]) > 0.9f)
                {
                    tangent[0] = 0.0f;
                    tangent[1] = 1.0f;
                }
                Float bitangent[3];
                bitangent[0] = coord.normal[1] * tangent[2] - coord.normal[2] * tangent[1];
                bitangent[1] = coord.normal[2] * tangent[0] - coord.normal[0] * tangent[2];
                bitangent[2] = coord.normal[0] * tangent[1] - coord.normal[1] * tangent[0];

                uint32_t seed = (uint32_t)(f * 199999ULL + i * 31337ULL + bounce * 7919ULL);
                size_t baseRayIdx = (size_t)cIdx * (size_t)raysPerLuxel;

                for (Int32 r = 0; r < raysPerLuxel; r++)
                {
                    seed = seed * 1664525u + 1013904223u;
                    Float u1 = ((Float)r + ((seed >> 16) & 0xFFFF) / 65536.0f) / (Float)raysPerLuxel;
                    seed = seed * 1664525u + 1013904223u;
                    Float u2 = ((seed >> 16) & 0xFFFF) / 65536.0f;

                    Float sVal, cVal;
                    g_fastTrig.SinCosFrac(u2, sVal, cVal);

                    Float rSqrt = sqrtf(std::clamp(u1, 0.0f, 1.0f));
                    Float x = rSqrt * cVal;
                    Float y = rSqrt * sVal;
                    Float z = sqrtf(std::max(0.0f, 1.0f - u1));

                    Float sampleDir[3] = {
                        tangent[0] * x + bitangent[0] * y + coord.normal[0] * z,
                        tangent[1] * x + bitangent[1] * y + coord.normal[1] * z,
                        tangent[2] * x + bitangent[2] * y + coord.normal[2] * z
                    };

                    gpu_ray_t& gray = gpuBounceRays[baseRayIdx + r];
                    gray.origin[0] = coord.worldPos[0];
                    gray.origin[1] = coord.worldPos[1];
                    gray.origin[2] = coord.worldPos[2];
                    gray.tMin = 0.01f;
                    gray.dir[0] = sampleDir[0];
                    gray.dir[1] = sampleDir[1];
                    gray.dir[2] = sampleDir[2];
                    gray.tMax = 4096.0f;
                }
            }

            const gpu_ray_hit_t* rawHits = TraceRayHitBatch(gpuBounceRays);

            if (rawHits)
            {
                #pragma omp parallel for schedule(static)
                for (int cIdx = 0; cIdx < (int)chunkSize; cIdx++)
                {
                    int idx = (int)(chunkStart + cIdx);
                    int f = activeLuxels[idx].f;
                    int i = activeLuxels[idx].i;
                    size_t baseRayIdx = (size_t)cIdx * (size_t)raysPerLuxel;
                    Float bounceAccum[3] = { 0.0f, 0.0f, 0.0f };

                    for (Int32 r = 0; r < raysPerLuxel; r++)
                    {
                        const auto& hit = rawHits[baseRayIdx + r];
                        if (hit.hit != 0)
                        {
                            Int32 hitFaceIdx = m_primToFaceMap[hit.primID];
                            if (hitFaceIdx >= 0 && hitFaceIdx < (Int32)m_faceInfos.size())
                            {
                                const auto& hitInfo = m_faceInfos[hitFaceIdx];
                                Float albedo[3];
                                SampleHitAlbedo(hit.primID, hit.u, hit.v, albedo);

                                bounceAccum[0] += (hitInfo.avgRadiance[0] * albedo[0] + hitInfo.emissive[0]);
                                bounceAccum[1] += (hitInfo.avgRadiance[1] * albedo[1] + hitInfo.emissive[1]);
                                bounceAccum[2] += (hitInfo.avgRadiance[2] * albedo[2] + hitInfo.emissive[2]);
                            }
                        }
                    }

                    Float rVal = bounceAccum[0] / (Float)raysPerLuxel;
                    Float gVal = bounceAccum[1] / (Float)raysPerLuxel;
                    Float bVal = bounceAccum[2] / (Float)raysPerLuxel;

                    stepBounce[f][i][0] = rVal;
                    stepBounce[f][i][1] = gVal;
                    stepBounce[f][i][2] = bVal;

                    faceLuxels[f][i].bounce[0][0] += rVal;
                    faceLuxels[f][i].bounce[0][1] += gVal;
                    faceLuxels[f][i].bounce[0][2] += bVal;
                }
            }
        }
    }

    OIDNDevice oidnDevice = oidnNewDevice(OIDN_DEVICE_TYPE_DEFAULT);
    oidnCommitDevice(oidnDevice);

    size_t maxPadW = 0;
    size_t maxPadH = 0;

    for (size_t f = 0; f < faceLightmaps.size(); f++)
    {
        const auto& lm = faceLightmaps[f];
        if (lm.totalLuxels <= 0 || faceLuxels[f].empty())
        {
            continue;
        }

        size_t padW = std::max(32, ((lm.luxelWidth + 15) / 16) * 16);
        size_t padH = std::max(32, ((lm.luxelHeight + 15) / 16) * 16);

        if (padW > maxPadW) 
            maxPadW = padW;
        if (padH > maxPadH)
            maxPadH = padH;
    }

    if (maxPadW != 0 && maxPadH != 0)
    {
        size_t maxFloats = maxPadW * maxPadH * 3;
        OIDNBuffer devBuf = oidnNewBuffer(oidnDevice, maxFloats * sizeof(float));
        std::vector<float> hostImg(maxFloats, 0.0f);

        int cachedFilterW = -1;
        int cachedFilterH = -1;
        OIDNFilter filter = nullptr;

        auto EnsureFilter = [&](int padW, int padH)
            {
                if (filter && cachedFilterW == padW && cachedFilterH == padH)
                {
                    return;
                }

                if (filter)
                {
                    oidnReleaseFilter(filter);
                    filter = nullptr;
                }

                filter = oidnNewFilter(oidnDevice, "RT");
                oidnSetFilterImage(filter, "color", devBuf, OIDN_FORMAT_FLOAT3, padW, padH, 0, 0, 0);
                oidnSetFilterImage(filter, "output", devBuf, OIDN_FORMAT_FLOAT3, padW, padH, 0, 0, 0);
                oidnSetFilterBool(filter, "hdr", true);
                oidnCommitFilter(filter);

                cachedFilterW = padW;
                cachedFilterH = padH;
            };

        for (size_t f = 0; f < faceLightmaps.size(); f++)
        {
            const auto& lm = faceLightmaps[f];
            if (lm.totalLuxels <= 0 || faceLuxels[f].empty())
            {
                continue;
            }

            int W = lm.luxelWidth;
            int H = lm.luxelHeight;
            int padW = std::max(32, ((W + 15) / 16) * 16);
            int padH = std::max(32, ((H + 15) / 16) * 16);
            size_t numFloats = (size_t)padW * padH * 3;

            for (int y = 0; y < padH; y++)
            {
                for (int x = 0; x < padW; x++)
                {
                    int srcIdx = std::clamp(y, 0, H - 1) * W + std::clamp(x, 0, W - 1);

                    for (int c = 0; c < 3; c++)
                    {
                        hostImg[(y * padW + x) * 3 + c] = faceLuxels[f][srcIdx].bounce[0][c];
                    }
                }
            }

            oidnWriteBuffer(devBuf, 0, numFloats * sizeof(float), hostImg.data());
            EnsureFilter(padW, padH);
            oidnExecuteFilter(filter);
            oidnReadBuffer(devBuf, 0, numFloats * sizeof(float), hostImg.data());

            for (int y = 0; y < H; y++)
            {
                for (int x = 0; x < W; x++)
                {
                    for (int c = 0; c < 3; c++)
                    {
                        faceLuxels[f][y * W + x].bounce[0][c] = std::max(0.0f, hostImg[(y * padW + x) * 3 + c]);
                    }
                }
            }
        }

        if (filter)
        {
            oidnReleaseFilter(filter);
        }

        oidnReleaseBuffer(devBuf);
    }

    oidnReleaseDevice(oidnDevice);

    struct face_bbox_t
    {
        Float mins[3];
        Float maxs[3];
    };
    std::vector<face_bbox_t> faceBBox(faceLightmaps.size());

    #pragma omp parallel for schedule(static)
    for (int f = 0; f < (int)faceLightmaps.size(); f++)
    {
        const auto& lm = faceLightmaps[f];
        auto& box = faceBBox[f];
        box.mins[0] = box.mins[1] = box.mins[2] = 1e9f;
        box.maxs[0] = box.maxs[1] = box.maxs[2] = -1e9f;

        for (Int32 i = 0; i < lm.totalLuxels; i++)
        {
            const Float* p = lm.sampleCoords[i].worldPos;
            for (int k = 0; k < 3; k++)
            {
                if (p[k] < box.mins[k]) box.mins[k] = p[k];
                if (p[k] > box.maxs[k]) box.maxs[k] = p[k];
            }
        }
    }

    std::unordered_map<Int32, std::vector<size_t>> planeFaceGroups;
    for (size_t f = 0; f < faceLightmaps.size(); f++)
    {
        if (!faceLuxels[f].empty() && faceLightmaps[f].totalLuxels > 0)
        {
            planeFaceGroups[faceLightmaps[f].planeIndex].push_back(f);
        }
    }

    std::vector<std::vector<size_t>> planeFaceLists;
    planeFaceLists.reserve(planeFaceGroups.size());
    for (auto& pair : planeFaceGroups)
    {
        if (pair.second.size() > 1)
        {
            planeFaceLists.push_back(std::move(pair.second));
        }
    }

    #pragma omp parallel for schedule(dynamic)
    for (int p = 0; p < (int)planeFaceLists.size(); p++)
    {
        const auto& group = planeFaceLists[p];
        size_t groupSize = group.size();
        std::vector<Int32> cand1, cand2;

        for (size_t idx1 = 0; idx1 < groupSize; idx1++)
        {
            size_t f1 = group[idx1];
            const auto& lm1 = faceLightmaps[f1];
            const auto& b1 = faceBBox[f1];

            for (size_t idx2 = idx1 + 1; idx2 < groupSize; idx2++)
            {
                size_t f2 = group[idx2];
                const auto& lm2 = faceLightmaps[f2];
                const auto& b2 = faceBBox[f2];

                if (b1.mins[0] > b2.maxs[0] + 1.0f || b1.maxs[0] < b2.mins[0] - 1.0f ||
                    b1.mins[1] > b2.maxs[1] + 1.0f || b1.maxs[1] < b2.mins[1] - 1.0f ||
                    b1.mins[2] > b2.maxs[2] + 1.0f || b1.maxs[2] < b2.mins[2] - 1.0f)
                {
                    continue;
                }

                Float overlapMins[3] = {
                    std::max(b1.mins[0], b2.mins[0]) - 1.0f,
                    std::max(b1.mins[1], b2.mins[1]) - 1.0f,
                    std::max(b1.mins[2], b2.mins[2]) - 1.0f
                };
                Float overlapMaxs[3] = {
                    std::min(b1.maxs[0], b2.maxs[0]) + 1.0f,
                    std::min(b1.maxs[1], b2.maxs[1]) + 1.0f,
                    std::min(b1.maxs[2], b2.maxs[2]) + 1.0f
                };

                cand1.clear();
                for (Int32 i1 = 0; i1 < lm1.totalLuxels; i1++)
                {
                    const Float* p1 = lm1.sampleCoords[i1].worldPos;
                    if (p1[0] >= overlapMins[0] && p1[0] <= overlapMaxs[0] &&
                        p1[1] >= overlapMins[1] && p1[1] <= overlapMaxs[1] &&
                        p1[2] >= overlapMins[2] && p1[2] <= overlapMaxs[2])
                    {
                        cand1.push_back(i1);
                    }
                }
                if (cand1.empty()) continue;

                cand2.clear();
                for (Int32 i2 = 0; i2 < lm2.totalLuxels; i2++)
                {
                    const Float* p2 = lm2.sampleCoords[i2].worldPos;
                    if (p2[0] >= overlapMins[0] && p2[0] <= overlapMaxs[0] &&
                        p2[1] >= overlapMins[1] && p2[1] <= overlapMaxs[1] &&
                        p2[2] >= overlapMins[2] && p2[2] <= overlapMaxs[2])
                    {
                        cand2.push_back(i2);
                    }
                }
                if (cand2.empty()) continue;

                for (Int32 i1 : cand1)
                {
                    const Float* p1 = lm1.sampleCoords[i1].worldPos;

                    for (Int32 i2 : cand2)
                    {
                        const Float* p2 = lm2.sampleCoords[i2].worldPos;
                        Float distSq = (p1[0] - p2[0]) * (p1[0] - p2[0]) +
                            (p1[1] - p2[1]) * (p1[1] - p2[1]) +
                            (p1[2] - p2[2]) * (p1[2] - p2[2]);

                        if (distSq < 1.0f)
                        {
                            for (Int32 s = 0; s < MBSPV1_MAX_LIGHTMAPS; s++)
                            {
                                for (Int32 c = 0; c < 3; c++)
                                {
                                    Float avgD = (faceLuxels[f1][i1].direct[s][c] + faceLuxels[f2][i2].direct[s][c]) * 0.5f;
                                    faceLuxels[f1][i1].direct[s][c] = faceLuxels[f2][i2].direct[s][c] = avgD;

                                    Float avgB = (faceLuxels[f1][i1].bounce[s][c] + faceLuxels[f2][i2].bounce[s][c]) * 0.5f;
                                    faceLuxels[f1][i1].bounce[s][c] = faceLuxels[f2][i2].bounce[s][c] = avgB;

                                    Float avgDir = (faceLuxels[f1][i1].dominantDir[s][c] + faceLuxels[f2][i2].dominantDir[s][c]) * 0.5f;
                                    faceLuxels[f1][i1].dominantDir[s][c] = faceLuxels[f2][i2].dominantDir[s][c] = avgDir;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    AllocateAllFaceLightmaps(faceLightmaps);

    size_t totalBytes = 0;
    for (const auto& lm : faceLightmaps)
    {
        if (lm.lightOffset >= 0)
        {
            Int32 numStyles = 0;
            for (Int32 s = 0; s < MBSPV1_MAX_LIGHTMAPS; s++)
            {
                if (g_BSP.GetFace(lm.bspFaceIndex).lmstyles[s] != 255)
                    numStyles++;
            }
            totalBytes += (size_t)lm.totalLuxels * sizeof(Float) * 3 * numStyles;
        }
    }

    std::vector<byte> defData(totalBytes, 0);
    std::vector<byte> ambData(totalBytes, 0);
    std::vector<byte> diffData(totalBytes, 0);
    std::vector<byte> vecData(totalBytes, 128);
    for (size_t vi = 2; vi < totalBytes; vi += 3)
    {
        vecData[vi] = 255;
    }

    #pragma omp parallel for schedule(dynamic)
    for (int f = 0; f < (int)faceLightmaps.size(); f++)
    {
        const auto& lm = faceLightmaps[f];
        if (lm.lightOffset < 0 || faceLuxels[f].empty())
        {
            continue;
        }

        const dmbspv1texinfo_t& tx = g_BSP.GetTexinfo(lm.texinfoIndex);
        const dmbspv1plane_t& pl = g_BSP.GetPlane(lm.planeIndex);

        auto Normalize = [](Float v[3])
            {
                Float len = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
                if (len > 0.00001f)
                {
                    v[0] /= len;
                    v[1] /= len;
                    v[2] /= len;
                }
            };

        Float tbn[3][3];
        for (Int32 k = 0; k < 3; k++)
        {
            tbn[0][k] = tx.vecs[0][k];
            tbn[1][k] = tx.vecs[1][k];
            tbn[2][k] = pl.normal[k];
        }

        Normalize(tbn[0]);
        Normalize(tbn[1]);
        Normalize(tbn[2]);

        Int32 numStyles = 0;
        for (Int32 s = 0; s < MBSPV1_MAX_LIGHTMAPS; s++)
        {
            if (g_BSP.GetFace(lm.bspFaceIndex).lmstyles[s] != 255)
                numStyles++;
        }

        auto Dot = [](const Float a[3], const Float b[3]) -> Float
            {
                return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
            };

        for (Int32 s = 0; s < numStyles; s++)
        {
            for (Int32 i = 0; i < lm.totalLuxels; i++)
            {
                const luxel_radiance_t& lux = faceLuxels[f][i];
                const luxel_coord_t& coord = lm.sampleCoords[i];

                Float T[3] = { tx.vecs[0][0], tx.vecs[0][1], tx.vecs[0][2] };
                Float B[3] = { tx.vecs[1][0], tx.vecs[1][1], tx.vecs[1][2] };
                Float N[3] = { coord.normal[0], coord.normal[1], coord.normal[2] };
                Normalize(N);

                Float dotTN = Dot(T, N);
                T[0] -= dotTN * N[0];
                T[1] -= dotTN * N[1];
                T[2] -= dotTN * N[2];
                Normalize(T);

                Float dotBN = Dot(B, N);
                B[0] -= dotBN * N[0];
                B[1] -= dotBN * N[1];
                B[2] -= dotBN * N[2];
                Normalize(B);

                Float finalTotal[3];
                Float finalDiffuse[3];
                Float finalAmbient[3];

                Float wDirLen = sqrtf(lux.dominantDir[s][0] * lux.dominantDir[s][0] + lux.dominantDir[s][1] * lux.dominantDir[s][1] + lux.dominantDir[s][2] * lux.dominantDir[s][2]);

                if (s == 0)
                {
                    finalTotal[0] = lux.direct[0][0] + lux.bounce[0][0] + lux.ambient[0];
                    finalTotal[1] = lux.direct[0][1] + lux.bounce[0][1] + lux.ambient[1];
                    finalTotal[2] = lux.direct[0][2] + lux.bounce[0][2] + lux.ambient[2];

                    finalDiffuse[0] = lux.direct[0][0];
                    finalDiffuse[1] = lux.direct[0][1];
                    finalDiffuse[2] = lux.direct[0][2];

                    finalAmbient[0] = lux.bounce[0][0] + lux.ambient[0];
                    finalAmbient[1] = lux.bounce[0][1] + lux.ambient[1];
                    finalAmbient[2] = lux.bounce[0][2] + lux.ambient[2];
                }
                else
                {
                    finalTotal[0] = lux.direct[s][0];
                    finalTotal[1] = lux.direct[s][1];
                    finalTotal[2] = lux.direct[s][2];

                    finalDiffuse[0] = lux.direct[s][0];
                    finalDiffuse[1] = lux.direct[s][1];
                    finalDiffuse[2] = lux.direct[s][2];

                    memset(finalAmbient, 0, sizeof(finalAmbient));
                }

                Float tangentDir[3] = { 0.0f, 0.0f, 1.0f };
                if (wDirLen > 0.001f)
                {
                    Float normWDir[3] = { lux.dominantDir[s][0] / wDirLen, lux.dominantDir[s][1] / wDirLen, lux.dominantDir[s][2] / wDirLen };
                    tangentDir[0] = Dot(normWDir, T);
                    tangentDir[1] = Dot(normWDir, B);
                    tangentDir[2] = Dot(normWDir, N);
                    Normalize(tangentDir);
                }

                size_t colorByteIdx = (size_t)lm.lightOffset + (s * lm.totalLuxels + i) * sizeof(Float) * 3;
                size_t vecByteIdx = (size_t)lm.lightOffset + (s * lm.totalLuxels + i) * 3;

                Float minL = (lm.bspFaceIndex >= 0 && lm.bspFaceIndex < (Int32)m_faceInfos.size()) ? m_faceInfos[lm.bspFaceIndex].minLight : 0.0f;

                auto ColorToHDR = [](Float val, Float minVal) -> Float
                    {
                        Float v = std::max(val, minVal);
                        if (v <= 0.0f) return 0.0f;
                        return powf(v / 128.0f, 0.55f) * 2.0f;
                    };

                Float* pDef = reinterpret_cast<Float*>(&defData[colorByteIdx]);
                Float* pAmb = reinterpret_cast<Float*>(&ambData[colorByteIdx]);
                Float* pDiff = reinterpret_cast<Float*>(&diffData[colorByteIdx]);

                for (Int32 c = 0; c < 3; c++)
                {
                    pDef[c] = ColorToHDR(finalTotal[c], minL);
                    pAmb[c] = ColorToHDR(finalAmbient[c], minL);
                    pDiff[c] = ColorToHDR(finalDiffuse[c], minL);
                }

                vecData[vecByteIdx + 0] = (byte)std::clamp((Int32)((tangentDir[0] * 0.5f + 0.5f) * 255.0f), 0, 255);
                vecData[vecByteIdx + 1] = (byte)std::clamp((Int32)((tangentDir[1] * 0.5f + 0.5f) * 255.0f), 0, 255);
                vecData[vecByteIdx + 2] = (byte)std::clamp((Int32)((tangentDir[2] * 0.5f + 0.5f) * 255.0f), 0, 255);
            }
        }

        if (lm.totalLuxels > 0)
        {
            Float sumRad[3] = { 0.0f, 0.0f, 0.0f };
            for (Int32 i = 0; i < lm.totalLuxels; i++)
            {
                sumRad[0] += faceLuxels[f][i].direct[0][0] + faceLuxels[f][i].bounce[0][0];
                sumRad[1] += faceLuxels[f][i].direct[0][1] + faceLuxels[f][i].bounce[0][1];
                sumRad[2] += faceLuxels[f][i].direct[0][2] + faceLuxels[f][i].bounce[0][2];
            }
            m_faceInfos[lm.bspFaceIndex].avgRadiance[0] = sumRad[0] / (Float)lm.totalLuxels;
            m_faceInfos[lm.bspFaceIndex].avgRadiance[1] = sumRad[1] / (Float)lm.totalLuxels;
            m_faceInfos[lm.bspFaceIndex].avgRadiance[2] = sumRad[2] / (Float)lm.totalLuxels;
        }
    }

    g_BSP.SetLightmapLayer(SURF_LIGHTMAP_DEFAULT, defData);
    g_BSP.SetLightmapLayer(SURF_LIGHTMAP_AMBIENT, ambData);
    g_BSP.SetLightmapLayer(SURF_LIGHTMAP_DIFFUSE, diffData);
    g_BSP.SetLightmapLayer(SURF_LIGHTMAP_VECTORS, vecData);
}