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

void CRadPipeline::BakeLightmaps(std::vector<lightmap_face_t>& faceLightmaps, const Char* baseDir, Int32 numBounces, Int32 raysPerLuxel)
{
    std::cout << "Baking lightmaps...\n";

    SmoothFaceNormals(faceLightmaps);

    struct luxel_radiance_t
    {
        Float direct[PBSPV3_MAX_LIGHTMAPS][3];
        Float bounce[PBSPV3_MAX_LIGHTMAPS][3];
        Float ambient[3];
        Float dominantDir[PBSPV3_MAX_LIGHTMAPS][3];
    };

    std::vector<std::vector<luxel_radiance_t>> faceLuxels(faceLightmaps.size());

    #pragma omp parallel for schedule(dynamic)
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
            const luxel_coord_t& coord = lm.sampleCoords[i];
            luxel_radiance_t& lux = faceLuxels[f][i];

            memset(&lux, 0, sizeof(lux));
            lux.ambient[0] = 0.00f;
            lux.ambient[1] = 0.00f;
            lux.ambient[2] = 0.00f;

            if (lm.bspFaceIndex >= 0 && lm.bspFaceIndex < (Int32)m_faceInfos.size())
            {
                lux.direct[0][0] += m_faceInfos[lm.bspFaceIndex].emissive[0];
                lux.direct[0][1] += m_faceInfos[lm.bspFaceIndex].emissive[1];
                lux.direct[0][2] += m_faceInfos[lm.bspFaceIndex].emissive[2];
            }

            for (const auto& lt : m_lights)
            {
                if (lt.type == LIGHT_SUN)
                {
                    Float sunTarget[3] = {
                        coord.worldPos[0] - lt.normal[0] * 32768.0f,
                        coord.worldPos[1] - lt.normal[1] * 32768.0f,
                        coord.worldPos[2] - lt.normal[2] * 32768.0f
                    };

                    Float NdotL = -(coord.normal[0] * lt.normal[0] + coord.normal[1] * lt.normal[1] + coord.normal[2] * lt.normal[2]);
                    if (NdotL <= 0.001f)
                    {
                        continue;
                    }

                    Float hitDist;
                    if (!TraceOcclusion(coord.worldPos, sunTarget, hitDist))
                    {
                        Float r = lt.color[0] * NdotL;
                        Float g = lt.color[1] * NdotL;
                        Float b = lt.color[2] * NdotL;

                        dpbspv3face_t& bspFace = g_BSP.GetFace(lm.bspFaceIndex);
                        Int32 styleSlot = -1;
                        for (Int32 s = 0; s < PBSPV3_MAX_LIGHTMAPS; s++)
                        {
                            if (bspFace.lmstyles[s] == lt.style)
                            {
                                styleSlot = s;
                                break;
                            }
                            if (bspFace.lmstyles[s] == 255)
                            {
                                bspFace.lmstyles[s] = lt.style;
                                styleSlot = s;
                                break;
                            }
                        }

                        if (styleSlot != -1)
                        {
                            lux.direct[styleSlot][0] += r;
                            lux.direct[styleSlot][1] += g;
                            lux.direct[styleSlot][2] += b;

                            lux.dominantDir[styleSlot][0] -= lt.normal[0] * NdotL;
                            lux.dominantDir[styleSlot][1] -= lt.normal[1] * NdotL;
                            lux.dominantDir[styleSlot][2] -= lt.normal[2] * NdotL;
                        }
                    }
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
                    if (lt.type == 2)
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

                    Float hitDist;
                    if (!TraceOcclusion(coord.worldPos, lt.origin, hitDist))
                    {
                        Float denom = (lt.falloff == 1) ? (dist * lt.fade) : (dist * dist * lt.fade);
                        Float atten = (1.0f / std::max(1.0f, denom)) * spotFactor * NdotL;

                        Float r = lt.color[0] * atten;
                        Float g = lt.color[1] * atten;
                        Float b = lt.color[2] * atten;

                        dpbspv3face_t& bspFace = g_BSP.GetFace(lm.bspFaceIndex);
                        Int32 styleSlot = -1;
                        for (Int32 s = 0; s < PBSPV3_MAX_LIGHTMAPS; s++)
                        {
                            if (bspFace.lmstyles[s] == lt.style)
                            {
                                styleSlot = s;
                                break;
                            }
                            if (bspFace.lmstyles[s] == 255)
                            {
                                bspFace.lmstyles[s] = lt.style;
                                styleSlot = s;
                                break;
                            }
                        }

                        if (styleSlot != -1)
                        {
                            lux.direct[styleSlot][0] += r;
                            lux.direct[styleSlot][1] += g;
                            lux.direct[styleSlot][2] += b;

                            lux.dominantDir[styleSlot][0] += dir[0] * NdotL;
                            lux.dominantDir[styleSlot][1] += dir[1] * NdotL;
                            lux.dominantDir[styleSlot][2] += dir[2] * NdotL;
                        }
                    }
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
                sumRad[0] += faceLuxels[f][i].direct[0][0];
                sumRad[1] += faceLuxels[f][i].direct[0][1];
                sumRad[2] += faceLuxels[f][i].direct[0][2];
            }
            m_faceInfos[lm.bspFaceIndex].avgRadiance[0] = sumRad[0] / (Float)lm.totalLuxels;
            m_faceInfos[lm.bspFaceIndex].avgRadiance[1] = sumRad[1] / (Float)lm.totalLuxels;
            m_faceInfos[lm.bspFaceIndex].avgRadiance[2] = sumRad[2] / (Float)lm.totalLuxels;
        }
    }

    for (Int32 bounce = 0; bounce < numBounces; bounce++)
    {
        std::vector<std::vector<std::array<Float, 3>>> stepBounce(faceLightmaps.size());

        #pragma omp parallel for schedule(dynamic)
        for (int f = 0; f < (int)faceLightmaps.size(); f++)
        {
            const auto& lm = faceLightmaps[f];
            if (g_BSP.GetTexinfo(lm.texinfoIndex).flags & 1)
            {
                continue;
            }

            stepBounce[f].resize(lm.totalLuxels);

            for (Int32 i = 0; i < lm.totalLuxels; i++)
            {
                const luxel_coord_t& coord = lm.sampleCoords[i];
                luxel_radiance_t& lux = faceLuxels[f][i];

                Float bounceAccum[3] = { 0.0f, 0.0f, 0.0f };

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

                for (Int32 r = 0; r < raysPerLuxel; r++)
                {
                    seed = seed * 1664525u + 1013904223u;
                    Float u1 = ((Float)r + ((seed >> 16) & 0xFFFF) / 65536.0f) / (Float)raysPerLuxel;
                    seed = seed * 1664525u + 1013904223u;
                    Float u2 = ((seed >> 16) & 0xFFFF) / 65536.0f;

                    Float rSqrt = sqrtf(std::clamp(u1, 0.0f, 1.0f));
                    Float theta = 2.0f * M_PI * u2;

                    Float x = rSqrt * cosf(theta);
                    Float y = rSqrt * sinf(theta);
                    Float z = sqrtf(std::max(0.0f, 1.0f - u1));

                    Float sampleDir[3] = {
                        tangent[0] * x + bitangent[0] * y + coord.normal[0] * z,
                        tangent[1] * x + bitangent[1] * y + coord.normal[1] * z,
                        tangent[2] * x + bitangent[2] * y + coord.normal[2] * z
                    };

                    ray_hit_t hit;
                    if (TraceRayHit(coord.worldPos, sampleDir, 4096.0f, hit))
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

                Float stepR = bounceAccum[0] / (Float)raysPerLuxel;
                Float stepG = bounceAccum[1] / (Float)raysPerLuxel;
                Float stepB = bounceAccum[2] / (Float)raysPerLuxel;

                stepBounce[f][i] = { stepR, stepG, stepB };

                lux.bounce[0][0] += stepR;
                lux.bounce[0][1] += stepG;
                lux.bounce[0][2] += stepB;
            }
        }

        for (size_t curF = 0; curF < faceLightmaps.size(); curF++)
        {
            const auto& lm = faceLightmaps[curF];
            if (!stepBounce[curF].empty() && lm.totalLuxels > 0 && lm.bspFaceIndex >= 0 && lm.bspFaceIndex < (Int32)m_faceInfos.size())
            {
                Float sumRad[3] = { 0.0f, 0.0f, 0.0f };
                for (Int32 li = 0; li < lm.totalLuxels; li++)
                {
                    sumRad[0] += stepBounce[curF][li][0];
                    sumRad[1] += stepBounce[curF][li][1];
                    sumRad[2] += stepBounce[curF][li][2];
                }
                m_faceInfos[lm.bspFaceIndex].avgRadiance[0] = sumRad[0] / (Float)lm.totalLuxels;
                m_faceInfos[lm.bspFaceIndex].avgRadiance[1] = sumRad[1] / (Float)lm.totalLuxels;
                m_faceInfos[lm.bspFaceIndex].avgRadiance[2] = sumRad[2] / (Float)lm.totalLuxels;
            }
        }
    }

    std::vector<std::vector<luxel_radiance_t>> filteredFaces = faceLuxels;
    const Float filterRadius = 32.0f;
    const Float filterRadiusSq = filterRadius * filterRadius;
    const Float twoSigmaSq = 2.0f * (16.0f * 16.0f);

    #pragma omp parallel for schedule(dynamic)
    for (int f1 = 0; f1 < (int)faceLightmaps.size(); f1++)
    {
        const auto& lm1 = faceLightmaps[f1];
        if (faceLuxels[f1].empty() || lm1.totalLuxels == 0) 
            continue;

        for (Int32 i1 = 0; i1 < lm1.totalLuxels; i1++)
        {
            const Float* p1 = lm1.sampleCoords[i1].worldPos;

            Float totalWeight = 0.0f;
            Float sumDirect[PBSPV3_MAX_LIGHTMAPS][3] = { 0.0f };
            Float sumBounce[PBSPV3_MAX_LIGHTMAPS][3] = { 0.0f };
            Float sumDir[PBSPV3_MAX_LIGHTMAPS][3] = { 0.0f };

            for (size_t f2 = 0; f2 < faceLightmaps.size(); f2++)
            {
                if (faceLuxels[f2].empty()) 
                    continue;

                if (faceLightmaps[f2].planeIndex != lm1.planeIndex) 
                    continue;

                const auto& lm2 = faceLightmaps[f2];
                for (Int32 i2 = 0; i2 < lm2.totalLuxels; i2++)
                {
                    const Float* p2 = lm2.sampleCoords[i2].worldPos;
                    Float distSq = (p1[0]-p2[0])*(p1[0]-p2[0]) + (p1[1]-p2[1])*(p1[1]-p2[1]) + (p1[2]-p2[2])*(p1[2]-p2[2]);

                    if (distSq <= filterRadiusSq)
                    {
                        Float w = expf(-distSq / twoSigmaSq);
                        for (Int32 s = 0; s < PBSPV3_MAX_LIGHTMAPS; s++)
                        {
                            for (Int32 c = 0; c < 3; c++)
                            {
                                sumDirect[s][c] += faceLuxels[f2][i2].direct[s][c] * w;
                                sumBounce[s][c] += faceLuxels[f2][i2].bounce[s][c] * w;
                                sumDir[s][c] += faceLuxels[f2][i2].dominantDir[s][c] * w;
                            }
                        }
                        totalWeight += w;
                    }
                }
            }

            if (totalWeight > 0.0001f)
            {
                for (Int32 s = 0; s < PBSPV3_MAX_LIGHTMAPS; s++)
                {
                    for (Int32 c = 0; c < 3; c++)
                    {
                        filteredFaces[f1][i1].direct[s][c] = sumDirect[s][c] / totalWeight;
                        filteredFaces[f1][i1].bounce[s][c] = sumBounce[s][c] / totalWeight;
                        filteredFaces[f1][i1].dominantDir[s][c] = sumDir[s][c] / totalWeight;
                    }
                }
            }
        }
    }
    faceLuxels = filteredFaces;

    for (size_t f1 = 0; f1 < faceLightmaps.size(); f1++)
    {
        const auto& lm1 = faceLightmaps[f1];
        if (faceLuxels[f1].empty()) 
            continue;

        for (size_t f2 = f1 + 1; f2 < faceLightmaps.size(); f2++)
        {
            const auto& lm2 = faceLightmaps[f2];
            if (faceLuxels[f2].empty()) 
                continue;

            if (lm1.planeIndex != lm2.planeIndex) 
                continue;

            for (Int32 i1 = 0; i1 < lm1.totalLuxels; i1++)
            {
                const Float* p1 = lm1.sampleCoords[i1].worldPos;

                for (Int32 i2 = 0; i2 < lm2.totalLuxels; i2++)
                {
                    const Float* p2 = lm2.sampleCoords[i2].worldPos;
                    Float distSq = (p1[0] - p2[0]) * (p1[0] - p2[0]) + (p1[1] - p2[1]) * (p1[1] - p2[1]) + (p1[2] - p2[2]) * (p1[2] - p2[2]);

                    if (distSq < 1.0f)
                    {
                        for (Int32 s = 0; s < PBSPV3_MAX_LIGHTMAPS; s++)
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

    AllocateAllFaceLightmaps(faceLightmaps);

    size_t totalBytes = 0;
    for (const auto& lm : faceLightmaps)
    {
        if (lm.lightOffset >= 0)
        {
            Int32 numStyles = 0;
            for (Int32 s = 0; s < PBSPV3_MAX_LIGHTMAPS; s++)
            {
                if (g_BSP.GetFace(lm.bspFaceIndex).lmstyles[s] != 255)
                    numStyles++;
            }
            totalBytes += (size_t)lm.totalLuxels * 3 * numStyles;
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

        const dpbspv3texinfo_t& tx = g_BSP.GetTexinfo(lm.texinfoIndex);
        const dpbspv3plane_t& pl = g_BSP.GetPlane(lm.planeIndex);

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
        for (Int32 s = 0; s < PBSPV3_MAX_LIGHTMAPS; s++)
        {
            if (g_BSP.GetFace(lm.bspFaceIndex).lmstyles[s] != 255)
                numStyles++;
        }

        auto Dot = [](const Float a[3], const Float b[3]) -> Float {
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
                Float nDotL = (wDirLen > 0.001f) ? (coord.normal[0] * (lux.dominantDir[s][0] / wDirLen) + coord.normal[1] * (lux.dominantDir[s][1] / wDirLen) + coord.normal[2] * (lux.dominantDir[s][2] / wDirLen)) : 0.0f;
                nDotL = std::clamp(nDotL, 0.0f, 1.0f);

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

                size_t baseIdx = (size_t)lm.lightOffset + (s * lm.totalLuxels + i) * 3;

                auto ColorToByte = [](Float colorComponent, Float minL = 0.0f) -> byte
                    {
                        Float scaled = colorComponent * 2.0f;
                        if (scaled < minL) scaled = minL;
                        if (scaled < 0.0f) scaled = 0.0f;
                        Float gammaAdjusted = powf(scaled / 256.0f, 0.55f) * 256.0f;
                        Int32 ival = (Int32)floorf(gammaAdjusted + 0.5f);
                        return (byte)std::clamp(ival, 0, 255);
                    };

                Float minL = (lm.bspFaceIndex >= 0 && lm.bspFaceIndex < (Int32)m_faceInfos.size()) ? m_faceInfos[lm.bspFaceIndex].minLight : 0.0f;

                defData[baseIdx + 0] = ColorToByte(finalTotal[0], minL);
                defData[baseIdx + 1] = ColorToByte(finalTotal[1], minL);
                defData[baseIdx + 2] = ColorToByte(finalTotal[2], minL);

                ambData[baseIdx + 0] = ColorToByte(finalAmbient[0], minL);
                ambData[baseIdx + 1] = ColorToByte(finalAmbient[1], minL);
                ambData[baseIdx + 2] = ColorToByte(finalAmbient[2], minL);

                diffData[baseIdx + 0] = ColorToByte(finalDiffuse[0], minL);
                diffData[baseIdx + 1] = ColorToByte(finalDiffuse[1], minL);
                diffData[baseIdx + 2] = ColorToByte(finalDiffuse[2], minL);

                vecData[baseIdx + 0] = (byte)std::clamp((Int32)((tangentDir[0] * 0.5f + 0.5f) * 255.0f), 0, 255);
                vecData[baseIdx + 1] = (byte)std::clamp((Int32)((tangentDir[1] * 0.5f + 0.5f) * 255.0f), 0, 255);
                vecData[baseIdx + 2] = (byte)std::clamp((Int32)((tangentDir[2] * 0.5f + 0.5f) * 255.0f), 0, 255);
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