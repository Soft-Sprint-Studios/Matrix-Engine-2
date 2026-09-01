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
#include "miniz.h"
#include <cmath>
#include <cstring>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>

CRadPipeline::CRadPipeline() :
    m_device(nullptr),
    m_scene(nullptr)
{
}

CRadPipeline::~CRadPipeline()
{
    Shutdown();
}

bool CRadPipeline::InitializeEmbree()
{
    m_device = rtcNewDevice(nullptr);
    if (!m_device)
    {
        std::cerr << "Error: Failed to initialize Embree 4 device.\n";
        return false;
    }

    m_scene = rtcNewScene(m_device);
    rtcSetSceneFlags(m_scene, RTC_SCENE_FLAG_ROBUST);
    rtcSetSceneBuildQuality(m_scene, RTC_BUILD_QUALITY_HIGH);
    return true;
}

void CRadPipeline::Shutdown()
{
    if (m_scene)
    {
        rtcReleaseScene(m_scene);
        m_scene = nullptr;
    }
    if (m_device)
    {
        rtcReleaseDevice(m_device);
        m_device = nullptr;
    }
}

void CRadPipeline::LoadTexlights(const Char* baseDir)
{
    m_texlights.clear();

    std::vector<std::string> pathsToTry;
    if (baseDir && baseDir[0])
    {
        pathsToTry.push_back(std::string(baseDir) + "/lights.rad");
    }
    pathsToTry.push_back("lights.rad");

    FILE* f = nullptr;
    for (const auto& path : pathsToTry)
    {
        f = fopen(path.c_str(), "r");
    }

    if (!f)
        return;

    char line[1024];
    while (fgets(line, sizeof(line), f))
    {
        char* comment = strstr(line, "//");
        if (comment)
            *comment = '\0';

        char texName[128];
        float r = 0, g = 0, b = 0, intensity = 1.0f;
        int args = sscanf(line, "%s %f %f %f %f", texName, &r, &g, &b, &intensity);

        if (args >= 4)
        {
            if (args == 5)
            {
                r *= (intensity / 255.0f);
                g *= (intensity / 255.0f);
                b *= (intensity / 255.0f);
            }
        }
        else if (args == 2)
        {
            g = b = r;
        }
        else
        {
            continue;
        }

        std::string key = texName;
        std::transform(key.begin(), key.end(), key.begin(), ::toupper);
        m_texlights[key] = { r, g, b };
    }

    fclose(f);
}

void CRadPipeline::BuildSceneGeometry(const map_data_t& mapData, const map_disp_data_t& dispData, const Char* baseDir)
{
    std::cout << "Building ray tracing scene...\n";

    LoadTexlights(baseDir);

    std::vector<Float> sceneVerts;
    std::vector<Uint32> sceneIndices;
    m_primToFaceMap.clear();
    m_scenePrims.clear();
    m_materials.clear();

    size_t faceCount = g_BSP.GetFaceCount();
    m_faceInfos.resize(faceCount);

    for (size_t i = 0; i < faceCount; i++)
    {
        const auto& f = g_BSP.GetFace(i);
        const auto& tx = g_BSP.GetTexinfo(f.texinfo);
        const auto& tex = g_BSP.GetTexture(tx.miptex);

        std::string texName = tex.name;
        if (m_materials.find(texName) == m_materials.end())
        {
            material_t mat;
            if (LoadMaterial(baseDir, tex.name, mat))
                m_materials[texName] = mat;
            else
            {
                mat.reflectScale = 1.0f;
                mat.hasAlphaTest = false;
                mat.hasNoShadow = false;
                mat.diffuseImage.width = 16;
                mat.diffuseImage.height = 16;
                m_materials[texName] = mat;
            }
        }

        const material_t& mat = m_materials[texName];
        m_faceInfos[i].reflectScale = mat.reflectScale;
        m_faceInfos[i].hasAlphaTest = mat.hasAlphaTest;
        m_faceInfos[i].diffuseImage = &mat.diffuseImage;
        m_faceInfos[i].minLight = 0.0f;
        m_faceInfos[i].ignoreNight = false;

        std::string upperTex = tex.name;
        std::transform(upperTex.begin(), upperTex.end(), upperTex.begin(), ::toupper);
        auto it = m_texlights.find(upperTex);
        if (it != m_texlights.end())
        {
            m_faceInfos[i].emissive[0] = it->second[0];
            m_faceInfos[i].emissive[1] = it->second[1];
            m_faceInfos[i].emissive[2] = it->second[2];
        }
        else
        {
            m_faceInfos[i].emissive[0] = 0.0f;
            m_faceInfos[i].emissive[1] = 0.0f;
            m_faceInfos[i].emissive[2] = 0.0f;
        }
    }

    for (const auto& ent : mapData.entities)
    {
        const Char* mdlName = ent.GetValue("model");
        if (!mdlName || mdlName[0] != '*') 
            continue;

        Int32 subIdx = atoi(mdlName + 1);
        if (subIdx <= 0 || subIdx >= (Int32)g_BSP.GetModelCount()) 
            continue;

        const auto& mdl = g_BSP.GetModel(subIdx);
        Float minL = (Float)atof(ent.GetValue("_minlight")) * 128.0f;
        bool ignN = (atoi(ent.GetValue("_ignorenight")) == 1);

        for (Int32 f = mdl.firstface; f < mdl.firstface + mdl.numfaces; f++)
        {
            if (f >= 0 && f < (Int32)m_faceInfos.size())
            {
                m_faceInfos[f].minLight = minL;
                m_faceInfos[f].ignoreNight = ignN;
            }
        }
    }

    for (size_t i = 0; i < faceCount; i++)
    {
        const dmbspv1face_t& f = g_BSP.GetFace(i);
        if (g_BSP.GetTexinfo(f.texinfo).flags & 1)
        {
            continue;
        }
        if (g_BSP.GetFaceDispIndex(i) >= 0)
        {
            continue;
        }

        if (g_BSP.GetModelCount() > 0)
        {
            const dmbspv1model_t& worldModel = g_BSP.GetModel(0);
            bool isWorldFace = ((Int32)i >= worldModel.firstface && (Int32)i < worldModel.firstface + worldModel.numfaces);

            if (!isWorldFace)
            {
                continue;
            }
        }

        Int32 firstEdge = f.firstedge;
        Int32 numEdges = f.numedges;
        if (numEdges < 3)
        {
            continue;
        }

        const dmbspv1texinfo_t& tx = g_BSP.GetTexinfo(f.texinfo);
        const auto& fInfo = m_faceInfos[i];
        Float texW = (fInfo.diffuseImage && fInfo.diffuseImage->width > 0) ? (Float)fInfo.diffuseImage->width : 1.0f;
        Float texH = (fInfo.diffuseImage && fInfo.diffuseImage->height > 0) ? (Float)fInfo.diffuseImage->height : 1.0f;

        std::vector<Uint32> faceVertIndices(numEdges);
        std::vector<std::array<Float, 2>> faceVertUVs(numEdges);

        for (Int32 e = 0; e < numEdges; e++)
        {
            Int32 surfEdge = g_BSP.GetSurfEdge(firstEdge + e);
            Uint32 vIdx = (surfEdge >= 0) ? g_BSP.GetEdge(surfEdge).vertexes[0] : g_BSP.GetEdge(-surfEdge).vertexes[1];
            faceVertIndices[e] = vIdx;

            const dmbspv1vertex_t& v = g_BSP.GetVertex(vIdx);
            Float s = v.origin[0] * tx.vecs[0][0] + v.origin[1] * tx.vecs[0][1] + v.origin[2] * tx.vecs[0][2] + tx.vecs[0][3];
            Float t = v.origin[0] * tx.vecs[1][0] + v.origin[1] * tx.vecs[1][1] + v.origin[2] * tx.vecs[1][2] + tx.vecs[1][3];

            faceVertUVs[e][0] = s / texW;
            faceVertUVs[e][1] = t / texH;
        }

        Uint32 rootVert = faceVertIndices[0];
        const dmbspv1vertex_t& v0 = g_BSP.GetVertex(rootVert);

        for (Int32 e = 1; e < numEdges - 1; e++)
        {
            const dmbspv1vertex_t& v1 = g_BSP.GetVertex(faceVertIndices[e]);
            const dmbspv1vertex_t& v2 = g_BSP.GetVertex(faceVertIndices[e + 1]);

            Uint32 baseIdx = (Uint32)(sceneVerts.size() / 3);

            sceneVerts.push_back(v0.origin[0]); sceneVerts.push_back(v0.origin[1]); sceneVerts.push_back(v0.origin[2]);
            sceneVerts.push_back(v1.origin[0]); sceneVerts.push_back(v1.origin[1]); sceneVerts.push_back(v1.origin[2]);
            sceneVerts.push_back(v2.origin[0]); sceneVerts.push_back(v2.origin[1]); sceneVerts.push_back(v2.origin[2]);

            sceneIndices.push_back(baseIdx + 0);
            sceneIndices.push_back(baseIdx + 1);
            sceneIndices.push_back(baseIdx + 2);
            m_primToFaceMap.push_back((Int32)i);

            scene_prim_t prim;
            prim.faceIndex = (Int32)i;
            prim.uv[0][0] = faceVertUVs[0][0]; prim.uv[0][1] = faceVertUVs[0][1];
            prim.uv[1][0] = faceVertUVs[e][0]; prim.uv[1][1] = faceVertUVs[e][1];
            prim.uv[2][0] = faceVertUVs[e + 1][0]; prim.uv[2][1] = faceVertUVs[e + 1][1];
            m_scenePrims.push_back(prim);
        }
    }

    for (const auto& di : dispData.displacements)
    {
        Int32 N = 1 << di.power;
        Int32 K = N + 1;
        std::vector<Float> grid(K * K * 3);

        for (Int32 y = 0; y < K; y++)
        {
            Float v = (Float)y / (Float)N;
            for (Int32 x = 0; x < K; x++)
            {
                Float u = (Float)x / (Float)N;

                Float base[3];
                for (Int32 c = 0; c < 3; c++)
                {
                    base[c] = (1.0f - u) * (1.0f - v) * di.corners[0][c] + u * (1.0f - v) * di.corners[1][c] + u * v * di.corners[2][c] + (1.0f - u) * v * di.corners[3][c];
                }

                Int32 idx = y * K + x;
                Float dist = (idx < (Int32)di.verts.size()) ? di.verts[idx].distance : 0.0f;
                const Float* vec = (idx < (Int32)di.verts.size()) ? di.verts[idx].vector : di.corners[0];

                grid[idx * 3 + 0] = base[0] + vec[0] * dist;
                grid[idx * 3 + 1] = base[1] + vec[1] * dist;
                grid[idx * 3 + 2] = base[2] + vec[2] * dist;
            }
        }

        for (Int32 y = 0; y < N; y++)
        {
            for (Int32 x = 0; x < N; x++)
            {
                Int32 i00 = (y * K + x) * 3;
                Int32 i10 = (y * K + (x + 1)) * 3;
                Int32 i01 = ((y + 1) * K + x) * 3;
                Int32 i11 = ((y + 1) * K + (x + 1)) * 3;

                Uint32 baseIdx = (Uint32)(sceneVerts.size() / 3);

                sceneVerts.push_back(grid[i00 + 0]); sceneVerts.push_back(grid[i00 + 1]); sceneVerts.push_back(grid[i00 + 2]);
                sceneVerts.push_back(grid[i01 + 0]); sceneVerts.push_back(grid[i01 + 1]); sceneVerts.push_back(grid[i01 + 2]);
                sceneVerts.push_back(grid[i11 + 0]); sceneVerts.push_back(grid[i11 + 1]); sceneVerts.push_back(grid[i11 + 2]);

                sceneIndices.push_back(baseIdx + 0);
                sceneIndices.push_back(baseIdx + 1);
                sceneIndices.push_back(baseIdx + 2);

                Int32 bspFaceIdx = g_BSP.ResolveFaceId(di.face_id);
                m_primToFaceMap.push_back(bspFaceIdx);
                scene_prim_t p0;
                p0.faceIndex = bspFaceIdx;

                if (bspFaceIdx >= 0)
                {
                    const dmbspv1face_t& bspFace = g_BSP.GetFace(bspFaceIdx);
                    const dmbspv1texinfo_t& tx = g_BSP.GetTexinfo(bspFace.texinfo);
                    const auto& fInfo = m_faceInfos[bspFaceIdx];
                    Float texW = (fInfo.diffuseImage && fInfo.diffuseImage->width > 0) ? (Float)fInfo.diffuseImage->width : 1.0f;
                    Float texH = (fInfo.diffuseImage && fInfo.diffuseImage->height > 0) ? (Float)fInfo.diffuseImage->height : 1.0f;

                    auto CalcUV = [&](Int32 gridX, Int32 gridY, Float outUV[2]) 
                        {
                        Int32 ptIdx = gridY * K + gridX;
                        Float posX = grid[ptIdx * 3 + 0];
                        Float posY = grid[ptIdx * 3 + 1];
                        Float posZ = grid[ptIdx * 3 + 2];
                        Float s = posX * tx.vecs[0][0] + posY * tx.vecs[0][1] + posZ * tx.vecs[0][2] + tx.vecs[0][3];
                        Float t = posX * tx.vecs[1][0] + posY * tx.vecs[1][1] + posZ * tx.vecs[1][2] + tx.vecs[1][3];
                        outUV[0] = s / texW;
                        outUV[1] = t / texH;
                        };

                    CalcUV(x, y, p0.uv[0]);
                    CalcUV(x, y + 1, p0.uv[1]);
                    CalcUV(x + 1, y + 1, p0.uv[2]);
                }
                else
                {
                    p0.uv[0][0] = 0.0f; p0.uv[0][1] = 0.0f;
                    p0.uv[1][0] = 0.0f; p0.uv[1][1] = 0.0f;
                    p0.uv[2][0] = 0.0f; p0.uv[2][1] = 0.0f;
                }
                m_scenePrims.push_back(p0);

                baseIdx += 3;

                sceneVerts.push_back(grid[i00 + 0]); sceneVerts.push_back(grid[i00 + 1]); sceneVerts.push_back(grid[i00 + 2]);
                sceneVerts.push_back(grid[i11 + 0]); sceneVerts.push_back(grid[i11 + 1]); sceneVerts.push_back(grid[i11 + 2]);
                sceneVerts.push_back(grid[i10 + 0]); sceneVerts.push_back(grid[i10 + 1]); sceneVerts.push_back(grid[i10 + 2]);

                sceneIndices.push_back(baseIdx + 0);
                sceneIndices.push_back(baseIdx + 1);
                sceneIndices.push_back(baseIdx + 2);
                m_primToFaceMap.push_back(bspFaceIdx);
                scene_prim_t p1;
                p1.faceIndex = bspFaceIdx;

                if (bspFaceIdx >= 0)
                {
                    const dmbspv1face_t& bspFace = g_BSP.GetFace(bspFaceIdx);
                    const dmbspv1texinfo_t& tx = g_BSP.GetTexinfo(bspFace.texinfo);
                    const auto& fInfo = m_faceInfos[bspFaceIdx];
                    Float texW = (fInfo.diffuseImage && fInfo.diffuseImage->width > 0) ? (Float)fInfo.diffuseImage->width : 1.0f;
                    Float texH = (fInfo.diffuseImage && fInfo.diffuseImage->height > 0) ? (Float)fInfo.diffuseImage->height : 1.0f;

                    auto CalcUV = [&](Int32 gridX, Int32 gridY, Float outUV[2]) 
                        {
                        Int32 ptIdx = gridY * K + gridX;
                        Float posX = grid[ptIdx * 3 + 0];
                        Float posY = grid[ptIdx * 3 + 1];
                        Float posZ = grid[ptIdx * 3 + 2];
                        Float s = posX * tx.vecs[0][0] + posY * tx.vecs[0][1] + posZ * tx.vecs[0][2] + tx.vecs[0][3];
                        Float t = posX * tx.vecs[1][0] + posY * tx.vecs[1][1] + posZ * tx.vecs[1][2] + tx.vecs[1][3];
                        outUV[0] = s / texW;
                        outUV[1] = t / texH;
                        };

                    CalcUV(x, y, p1.uv[0]);
                    CalcUV(x + 1, y + 1, p1.uv[1]);
                    CalcUV(x + 1, y, p1.uv[2]);
                }
                else
                {
                    p1.uv[0][0] = 0.0f; 
                    p1.uv[0][1] = 0.0f;
                    p1.uv[1][0] = 0.0f; 
                    p1.uv[1][1] = 0.0f;
                    p1.uv[2][0] = 0.0f; 
                    p1.uv[2][1] = 0.0f;
                }
                m_scenePrims.push_back(p1);
            }
        }
    }

    for (const auto& ent : mapData.entities)
    {
        Int32 lightFlags = atoi(ent.GetValue("zhlt_lightflags"));
        if ((lightFlags & 2) != 0)
        {
            for (const auto& br : ent.brushes)
            {
                poly_brush_t pb;
                if (!BuildBrushPolygons(br, pb))
                    continue;

                for (const auto& f : pb.faces)
                {
                    if (f.verts.size() < 3) 
                        continue;

                    Uint32 bIdx = (Uint32)(sceneVerts.size() / 3);
                    const auto& v0 = f.verts[0];
                    for (size_t e = 1; e < f.verts.size() - 1; e++)
                    {
                        const auto& v1 = f.verts[e];
                        const auto& v2 = f.verts[e + 1];
                        sceneVerts.push_back(v0.pos[0]); sceneVerts.push_back(v0.pos[1]); sceneVerts.push_back(v0.pos[2]);
                        sceneVerts.push_back(v1.pos[0]); sceneVerts.push_back(v1.pos[1]); sceneVerts.push_back(v1.pos[2]);
                        sceneVerts.push_back(v2.pos[0]); sceneVerts.push_back(v2.pos[1]); sceneVerts.push_back(v2.pos[2]);

                        sceneIndices.push_back(bIdx + 0);
                        sceneIndices.push_back(bIdx + 1);
                        sceneIndices.push_back(bIdx + 2);
                        m_primToFaceMap.push_back(-1);

                        scene_prim_t p;
                        p.faceIndex = -1;
                        p.uv[0][0] = 0.0f; p.uv[0][1] = 0.0f;
                        p.uv[1][0] = 0.0f; p.uv[1][1] = 0.0f;
                        p.uv[2][0] = 0.0f; p.uv[2][1] = 0.0f;
                        m_scenePrims.push_back(p);
                        bIdx += 3;
                    }
                }
            }
        }

        if (strcmp(ent.GetValue("classname"), "env_model") != 0)
            continue;
        if (atoi(ent.GetValue("disableshadows")) != 0)
            continue;

        const Char* modelPath = ent.GetValue("model");
        if (!modelPath || !modelPath[0])
            continue;

        Float origin[3] = { 0.0f, 0.0f, 0.0f };
        Float angles[3] = { 0.0f, 0.0f, 0.0f };
        Float scale = (Float)atof(ent.GetValue("scale"));
        if (scale <= 0.0f) 
            scale = 1.0f;

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
                continue;
        }

        Uint32 baseVertOffset = (Uint32)(sceneVerts.size() / 3);
        for (size_t vi = 0; vi < vbm.worldVerts.size(); vi++)
        {
            sceneVerts.push_back(vbm.worldVerts[vi]);
        }

        for (size_t ii = 0; ii < vbm.sceneIndices.size(); ii += 3)
        {
            sceneIndices.push_back(baseVertOffset + vbm.sceneIndices[ii + 0]);
            sceneIndices.push_back(baseVertOffset + vbm.sceneIndices[ii + 1]);
            sceneIndices.push_back(baseVertOffset + vbm.sceneIndices[ii + 2]);
            m_primToFaceMap.push_back(-1);

            scene_prim_t p;
            p.faceIndex = -1;
            p.uv[0][0] = 0.0f; p.uv[0][1] = 0.0f;
            p.uv[1][0] = 0.0f; p.uv[1][1] = 0.0f;
            p.uv[2][0] = 0.0f; p.uv[2][1] = 0.0f;
            m_scenePrims.push_back(p);
        }
    }

    if (sceneVerts.empty() || sceneIndices.empty())
    {
        return;
    }

    RTCGeometry geom = rtcNewGeometry(m_device, RTC_GEOMETRY_TYPE_TRIANGLE);

    Float* vb = (Float*)rtcSetNewGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, sizeof(Float) * 3, sceneVerts.size() / 3);
    memcpy(vb, sceneVerts.data(), sceneVerts.size() * sizeof(Float));

    Uint32* ib = (Uint32*)rtcSetNewGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, sizeof(Uint32) * 3, sceneIndices.size() / 3);
    memcpy(ib, sceneIndices.data(), sceneIndices.size() * sizeof(Uint32));

    rtcSetGeometryUserData(geom, this);
    rtcSetGeometryIntersectFilterFunction(geom, AlphaTestFilterCallback);
    rtcSetGeometryOccludedFilterFunction(geom, AlphaTestFilterCallback);
    rtcCommitGeometry(geom);
    rtcAttachGeometry(m_scene, geom);
    rtcReleaseGeometry(geom);
    rtcCommitScene(m_scene);
}

bool CRadPipeline::TraceOcclusion(const Float start[3], const Float end[3], Float& outDist) const
{
    Float dir[3] = { end[0] - start[0], end[1] - start[1], end[2] - start[2] };
    Float dist = sqrtf(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    if (dist < 0.001f)
    {
        return false;
    }

    dir[0] /= dist;
    dir[1] /= dist;
    dir[2] /= dist;

    outDist = dist;

    RTCRay ray;
    ray.org_x = start[0];
    ray.org_y = start[1];
    ray.org_z = start[2];
    ray.dir_x = dir[0];
    ray.dir_y = dir[1];
    ray.dir_z = dir[2];
    ray.tnear = 0.01f;
    ray.tfar = dist - 0.01f;
    ray.time = 0.0f;
    ray.mask = (unsigned int)-1;
    ray.flags = 0;

    RTCOccludedArguments args;
    rtcInitOccludedArguments(&args);
    args.flags = RTC_RAY_QUERY_FLAG_INVOKE_ARGUMENT_FILTER;

    rtcOccluded1(m_scene, &ray, &args);
    return ray.tfar < 0.0f;
}

bool CRadPipeline::TraceRayHit(const Float start[3], const Float dir[3], Float maxDist, ray_hit_t& outHit) const
{
    RTCRayHit rayhit;
    rayhit.ray.org_x = start[0];
    rayhit.ray.org_y = start[1];
    rayhit.ray.org_z = start[2];
    rayhit.ray.dir_x = dir[0];
    rayhit.ray.dir_y = dir[1];
    rayhit.ray.dir_z = dir[2];
    rayhit.ray.tnear = 0.01f;
    rayhit.ray.tfar = maxDist;
    rayhit.ray.time = 0.0f;
    rayhit.ray.mask = (unsigned int)-1;
    rayhit.ray.flags = 0;

    rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
    rayhit.hit.primID = RTC_INVALID_GEOMETRY_ID;

    RTCIntersectArguments args;
    rtcInitIntersectArguments(&args);
    args.flags = RTC_RAY_QUERY_FLAG_INVOKE_ARGUMENT_FILTER;

    rtcIntersect1(m_scene, &rayhit, &args);

    if (rayhit.hit.geomID != RTC_INVALID_GEOMETRY_ID)
    {
        outHit.hit = true;
        outHit.dist = rayhit.ray.tfar;
        outHit.normal[0] = rayhit.hit.Ng_x;
        outHit.normal[1] = rayhit.hit.Ng_y;
        outHit.normal[2] = rayhit.hit.Ng_z;

        Float nLen = sqrtf(outHit.normal[0] * outHit.normal[0] + outHit.normal[1] * outHit.normal[1] + outHit.normal[2] * outHit.normal[2]);
        if (nLen > 0.0001f)
        {
            outHit.normal[0] /= nLen;
            outHit.normal[1] /= nLen;
            outHit.normal[2] /= nLen;
        }

        outHit.geomID = rayhit.hit.geomID;
        outHit.primID = rayhit.hit.primID;
        outHit.u = rayhit.hit.u;
        outHit.v = rayhit.hit.v;
        return true;
    }

    outHit.hit = false;
    return false;
}

void CRadPipeline::SampleHitAlbedo(Uint32 primID, Float u, Float v, Float outAlbedo[3]) const
{
    outAlbedo[0] = 0.5f;
    outAlbedo[1] = 0.5f;
    outAlbedo[2] = 0.5f;

    if (primID >= m_scenePrims.size())
        return;

    const scene_prim_t& prim = m_scenePrims[primID];
    if (prim.faceIndex < 0 || prim.faceIndex >= (Int32)m_faceInfos.size())
        return;

    const face_info_t& fInfo = m_faceInfos[prim.faceIndex];
    if (!fInfo.diffuseImage || fInfo.diffuseImage->rgba.empty() || fInfo.diffuseImage->width <= 0 || fInfo.diffuseImage->height <= 0)
        return;

    Float w = 1.0f - u - v;
    Float texU = w * prim.uv[0][0] + u * prim.uv[1][0] + v * prim.uv[2][0];
    Float texV = w * prim.uv[0][1] + u * prim.uv[1][1] + v * prim.uv[2][1];

    texU = texU - floorf(texU);
    texV = texV - floorf(texV);

    Int32 px = std::clamp((Int32)(texU * fInfo.diffuseImage->width), 0, fInfo.diffuseImage->width - 1);
    Int32 py = std::clamp((Int32)(texV * fInfo.diffuseImage->height), 0, fInfo.diffuseImage->height - 1);

    size_t pixelOffset = ((size_t)py * fInfo.diffuseImage->width + px) * 4;
    Float r = (Float)fInfo.diffuseImage->rgba[pixelOffset + 0] / 255.0f;
    Float g = (Float)fInfo.diffuseImage->rgba[pixelOffset + 1] / 255.0f;
    Float b = (Float)fInfo.diffuseImage->rgba[pixelOffset + 2] / 255.0f;

    outAlbedo[0] = powf(r, 2.2f) * fInfo.reflectScale;
    outAlbedo[1] = powf(g, 2.2f) * fInfo.reflectScale;
    outAlbedo[2] = powf(b, 2.2f) * fInfo.reflectScale;
}