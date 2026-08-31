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
#include "vbm.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <iostream>
#include <omp.h>

void CRadPipeline::BakeVertexLights(map_data_t& mapData, const Char* baseDir)
{
    std::cout << "Baking model vertex lighting...\n";

    std::vector<byte> totalAmbient;
    std::vector<byte> totalDiffuse;
    std::vector<byte> totalVectors;

    for (auto& ent : mapData.entities)
    {
        const Char* classname = ent.GetValue("classname");
        if (strcmp(classname, "env_model") != 0)
            continue;

        const Char* modelPath = ent.GetValue("model");
        if (!modelPath || !modelPath[0])
            continue;

        Float origin[3] = { 0.0f, 0.0f, 0.0f };
        Float angles[3] = { 0.0f, 0.0f, 0.0f };
        Float scale = (Float)atof(ent.GetValue("scale"));
        if (scale <= 0.0f) scale = 1.0f;

        sscanf(ent.GetValue("origin"), "%f %f %f", &origin[0], &origin[1], &origin[2]);
        if (ent.GetValue("angles")[0])
        {
            sscanf(ent.GetValue("angles"), "%f %f %f", &angles[0], &angles[1], &angles[2]);
        }
        else if (ent.GetValue("angle")[0])
        {
            Float a = (Float)atof(ent.GetValue("angle"));
            if (a == -1.0f) 
                angles[0] = -90.0f;
            else if (a == -2.0f) 
                angles[0] = 90.0f;
            else 
                angles[1] = a;
        }

        angles[1] += 90.0f;

        Char fullVbmPath[512];
        snprintf(fullVbmPath, sizeof(fullVbmPath), "%s/%s", baseDir, modelPath);
        char* ext = strstr(fullVbmPath, ".mdl");
        if (ext) 
            strcpy(ext, ".vbm");

        vbm_model_t vbm;
        if (!LoadVBMModel(fullVbmPath, origin, angles, scale, vbm))
        {
            snprintf(fullVbmPath, sizeof(fullVbmPath), "%s/models/%s", baseDir, modelPath);
            ext = strstr(fullVbmPath, ".mdl");
            if (ext) 
                strcpy(ext, ".vbm");

            if (!LoadVBMModel(fullVbmPath, origin, angles, scale, vbm))
            {
                continue;
            }
        }

        Int32 vertexCount = vbm.numVerts;
        Int32 bufferOffset = (Int32)totalAmbient.size();

        std::vector<byte> modelAmbient(vertexCount * 3, 0);
        std::vector<byte> modelDiffuse(vertexCount * 3, 0);
        std::vector<byte> modelVectors(vertexCount * 3, 128);
        for (size_t vi = 2; vi < modelVectors.size(); vi += 3)
        {
            modelVectors[vi] = 255;
        }

        auto ColorToByte = [](Float colorComponent) -> byte
        {
            Float scaled = colorComponent * 2.0f;
            if (scaled < 0.0f) scaled = 0.0f;
            Float gammaAdjusted = powf(scaled / 256.0f, 0.55f) * 256.0f;
            Int32 ival = (Int32)floorf(gammaAdjusted + 0.5f);
            return (byte)std::clamp(ival, 0, 255);
        };

        struct vert_style_data_t
        {
            Float direct[64][3];
            Float dominantDir[64][3];
            Float ambient[3];
        };

        std::vector<vert_style_data_t> vertSamples(vertexCount);
        Float maxLightPerStyle[64] = { 0.0f };

        #pragma omp parallel for schedule(dynamic)
        for (Int32 v = 0; v < vertexCount; v++)
        {
            Float norm[3] = { vbm.worldNormals[v * 3 + 0], vbm.worldNormals[v * 3 + 1], vbm.worldNormals[v * 3 + 2] };
            Float pos[3] = {
                vbm.worldVerts[v * 3 + 0] + norm[0] * 1.0f,
                vbm.worldVerts[v * 3 + 1] + norm[1] * 1.0f,
                vbm.worldVerts[v * 3 + 2] + norm[2] * 1.0f
            };
            vert_style_data_t& vs = vertSamples[v];
            memset(&vs, 0, sizeof(vs));

            for (const auto& lt : m_lights)
            {
                Int32 style = (lt.style >= 0 && lt.style < 64) ? lt.style : 0;

                if (lt.type == LIGHT_SUN)
                {
                    Float sunTarget[3] = {
                        pos[0] - lt.normal[0] * 32768.0f,
                        pos[1] - lt.normal[1] * 32768.0f,
                        pos[2] - lt.normal[2] * 32768.0f
                    };

                    Float hitDist;
                    if (!TraceOcclusion(pos, sunTarget, hitDist))
                    {
                        vs.direct[style][0] += lt.color[0];
                        vs.direct[style][1] += lt.color[1];
                        vs.direct[style][2] += lt.color[2];

                        Float maxC = std::max({ lt.color[0], lt.color[1], lt.color[2] });
                        vs.dominantDir[style][0] -= lt.normal[0] * maxC;
                        vs.dominantDir[style][1] -= lt.normal[1] * maxC;
                        vs.dominantDir[style][2] -= lt.normal[2] * maxC;
                    }
                }
                else if (lt.type == LIGHT_POINT || lt.type == LIGHT_SPOT)
                {
                    Float toLight[3] = { lt.origin[0] - pos[0], lt.origin[1] - pos[1], lt.origin[2] - pos[2] };
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

                    Float hitDist;
                    if (!TraceOcclusion(pos, lt.origin, hitDist))
                    {
                        Float denom = (lt.falloff == 1) ? (dist * lt.fade) : (dist * dist * lt.fade);
                        Float atten = (1.0f / std::max(1.0f, denom)) * spotFactor;

                        Float r = lt.color[0] * atten;
                        Float g = lt.color[1] * atten;
                        Float b = lt.color[2] * atten;

                        vs.direct[style][0] += r;
                        vs.direct[style][1] += g;
                        vs.direct[style][2] += b;

                        Float maxC = std::max({ r, g, b });
                        vs.dominantDir[style][0] += dir[0] * maxC;
                        vs.dominantDir[style][1] += dir[1] * maxC;
                        vs.dominantDir[style][2] += dir[2] * maxC;
                    }
                }
            }

            const Int32 numBounceRays = 8;
            Float bounceRad[3] = { 0.0f, 0.0f, 0.0f };
            Float tangent[3] = { 1.0f, 0.0f, 0.0f };
            if (fabsf(norm[0]) > 0.9f) 
            { 
                tangent[0] = 0.0f; 
                tangent[1] = 1.0f; 
            }
            Float bitangent[3] = { norm[1] * tangent[2] - norm[2] * tangent[1], norm[2] * tangent[0] - norm[0] * tangent[2], norm[0] * tangent[1] - norm[1] * tangent[0]};
            Float bounceDir[3] = { 0.0f, 0.0f, 0.0f };

            for (Int32 r = 0; r < numBounceRays; r++)
            {
                Float u1 = ((Float)r + 0.5f) / (Float)numBounceRays;
                Float u2 = (Float)((r * 1664525u + 1013904223u) & 0xFFFF) / 65536.0f;
                Float rSqrt = sqrtf(u1);
                Float theta = 2.0f * M_PI * u2;
                Float x = rSqrt * cosf(theta);
                Float y = rSqrt * sinf(theta);
                Float z = sqrtf(std::max(0.0f, 1.0f - u1));
                Float sampleDir[3] = {tangent[0] * x + bitangent[0] * y + norm[0] * z, tangent[1] * x + bitangent[1] * y + norm[1] * z, tangent[2] * x + bitangent[2] * y + norm[2] * z};

                ray_hit_t hit;
                if (TraceRayHit(pos, sampleDir, 2048.0f, hit))
                {
                    Int32 hitFace = m_primToFaceMap[hit.primID];
                    if (hitFace >= 0 && hitFace < (Int32)m_faceInfos.size())
                    {
                        Float rVal = (m_faceInfos[hitFace].avgRadiance[0] * m_faceInfos[hitFace].reflectivity[0] + m_faceInfos[hitFace].emissive[0]);
                        Float gVal = (m_faceInfos[hitFace].avgRadiance[1] * m_faceInfos[hitFace].reflectivity[1] + m_faceInfos[hitFace].emissive[1]);
                        Float bVal = (m_faceInfos[hitFace].avgRadiance[2] * m_faceInfos[hitFace].reflectivity[2] + m_faceInfos[hitFace].emissive[2]);

                        bounceRad[0] += rVal;
                        bounceRad[1] += gVal;
                        bounceRad[2] += bVal;

                        Float maxC = std::max({ rVal, gVal, bVal });
                        bounceDir[0] += sampleDir[0] * maxC;
                        bounceDir[1] += sampleDir[1] * maxC;
                        bounceDir[2] += sampleDir[2] * maxC;
                    }
                }
            }

            vs.ambient[0] = (bounceRad[0] / (Float)numBounceRays) * M_PI;
            vs.ambient[1] = (bounceRad[1] / (Float)numBounceRays) * M_PI;
            vs.ambient[2] = (bounceRad[2] / (Float)numBounceRays) * M_PI;

            Float dDirLen = sqrtf(vs.dominantDir[0][0] * vs.dominantDir[0][0] + vs.dominantDir[0][1] * vs.dominantDir[0][1] + vs.dominantDir[0][2] * vs.dominantDir[0][2]);
            if (dDirLen <= 0.001f)
            {
                vs.dominantDir[0][0] = bounceDir[0];
                vs.dominantDir[0][1] = bounceDir[1];
                vs.dominantDir[0][2] = bounceDir[2];
            }
        }

        for (Int32 s = 0; s < 64; s++)
        {
            for (Int32 v = 0; v < vertexCount; v++)
            {
                Float maxC = std::max({ vertSamples[v].direct[s][0], vertSamples[v].direct[s][1], vertSamples[v].direct[s][2] });
                if (maxC > maxLightPerStyle[s])
                    maxLightPerStyle[s] = maxC;
            }
        }

        byte assignedStyles[PBSPV3_MAX_LIGHTMAPS];
        memset(assignedStyles, 255, sizeof(assignedStyles));
        assignedStyles[0] = 0;

        for (Int32 slot = 1; slot < PBSPV3_MAX_LIGHTMAPS; slot++)
        {
            Int32 bestStyle = -1;
            Float bestIntensity = 0.1f;

            for (Int32 s = 1; s < 64; s++)
            {
                if (maxLightPerStyle[s] > bestIntensity)
                {
                    bestIntensity = maxLightPerStyle[s];
                    bestStyle = s;
                }
            }

            if (bestStyle != -1)
            {
                assignedStyles[slot] = (byte)bestStyle;
                maxLightPerStyle[bestStyle] = 0.0f;
            }
        }

        Int32 activeStylesCount = 0;
        for (Int32 s = 0; s < PBSPV3_MAX_LIGHTMAPS; s++)
        {
            if (assignedStyles[s] != 255)
                activeStylesCount++;
        }

        for (Int32 slot = 0; slot < activeStylesCount; slot++)
        {
            Int32 style = assignedStyles[slot];
            std::vector<byte> slotAmbient(vertexCount * 3, 0);
            std::vector<byte> slotDiffuse(vertexCount * 3, 0);
            std::vector<byte> slotVectors(vertexCount * 3, 128);
            for (size_t vi = 2; vi < slotVectors.size(); vi += 3)
            {
                slotVectors[vi] = 255;
            }

            for (Int32 v = 0; v < vertexCount; v++)
            {
                const auto& vs = vertSamples[v];
                if (slot == 0)
                {
                    slotAmbient[v * 3 + 0] = ColorToByte(vs.ambient[0]);
                    slotAmbient[v * 3 + 1] = ColorToByte(vs.ambient[1]);
                    slotAmbient[v * 3 + 2] = ColorToByte(vs.ambient[2]);
                }

                slotDiffuse[v * 3 + 0] = ColorToByte(vs.direct[style][0]);
                slotDiffuse[v * 3 + 1] = ColorToByte(vs.direct[style][1]);
                slotDiffuse[v * 3 + 2] = ColorToByte(vs.direct[style][2]);

                Float dLen = sqrtf(vs.dominantDir[style][0] * vs.dominantDir[style][0] + vs.dominantDir[style][1] * vs.dominantDir[style][1] + vs.dominantDir[style][2] * vs.dominantDir[style][2]);
                Float normD[3] = { 0.0f, 0.0f, 1.0f };
                if (dLen > 0.001f)
                {
                    normD[0] = vs.dominantDir[style][0] / dLen;
                    normD[1] = vs.dominantDir[style][1] / dLen;
                    normD[2] = vs.dominantDir[style][2] / dLen;
                }

                slotVectors[v * 3 + 0] = (byte)std::clamp((Int32)((normD[0] * 0.5f + 0.5f) * 255.0f), 0, 255);
                slotVectors[v * 3 + 1] = (byte)std::clamp((Int32)((normD[1] * 0.5f + 0.5f) * 255.0f), 0, 255);
                slotVectors[v * 3 + 2] = (byte)std::clamp((Int32)((normD[2] * 0.5f + 0.5f) * 255.0f), 0, 255);
            }

            totalAmbient.insert(totalAmbient.end(), slotAmbient.begin(), slotAmbient.end());
            totalDiffuse.insert(totalDiffuse.end(), slotDiffuse.begin(), slotDiffuse.end());
            totalVectors.insert(totalVectors.end(), slotVectors.begin(), slotVectors.end());
        }

        auto SetOrAddKey = [](map_entity_t& e, const std::string& key, const std::string& val)
            {
                for (auto& ep : e.epairs)
                {
                    if (ep.key == key) 
                    { 
                        ep.value = val; 
                        return; 
                    }
                }
                e.epairs.push_back({ key, val });
            };

        SetOrAddKey(ent, "vlight_offset", std::to_string(bufferOffset));
        SetOrAddKey(ent, "vlight_hash", vbm.vertexHash);
        SetOrAddKey(ent, "vlight_vertexcount", std::to_string(vertexCount));

        std::string styleString = std::to_string(assignedStyles[0]) + ";" + std::to_string(assignedStyles[1]) + ";" + std::to_string(assignedStyles[2]) + ";" + std::to_string(assignedStyles[3]);
        SetOrAddKey(ent, "vlight_styles", styleString);
    }

    g_BSP.SetVertexLightingLayer(VERTEX_LIGHTING_AMBIENT, totalAmbient);
    g_BSP.SetVertexLightingLayer(VERTEX_LIGHTING_DIFFUSE, totalDiffuse);
    g_BSP.SetVertexLightingLayer(VERTEX_LIGHTING_VECTORS, totalVectors);
}