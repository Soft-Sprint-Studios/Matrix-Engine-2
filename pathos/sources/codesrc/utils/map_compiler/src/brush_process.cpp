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
#include "brush.h"
#include "bsp.h"
#include "lightmap.h"
#include <cmath>
#include <cstring>
#include <sstream>
#include <algorithm>

static void InitializeLeaf0()
{
    Int32 solidMins[3] = { -999999, -999999, -999999 };
    Int32 solidMaxs[3] = { 999999, 999999, 999999 };
    g_BSP.InsertLeaf(CONTENTS_SOLID, -1, solidMins, solidMaxs, 0, 0, 0, 0);
}

static void EmitModelFaces(const std::vector<map_brush_t>& brushes, Int32& outFirstFace, Int32& outNumFaces, Float outMins[3], Float outMaxs[3])
{
    outFirstFace = (Int32)g_BSP.GetFaceCount();
    outNumFaces = 0;
    outMins[0] = outMins[1] = outMins[2] = 9999999.0f;
    outMaxs[0] = outMaxs[1] = outMaxs[2] = -9999999.0f;

    for (const auto& brush : brushes)
    {
        poly_brush_t poly;
        if (!BuildBrushPolygons(brush, poly))
            continue;

        for (Int32 k = 0; k < 3; k++)
        {
            if (poly.mins[k] < outMins[k]) outMins[k] = poly.mins[k];
            if (poly.maxs[k] > outMaxs[k]) outMaxs[k] = poly.maxs[k];
        }
    }
}

bool ProcessMapGeometry(map_data_t& mapData, const map_disp_data_t& dispData, std::vector<lightmap_face_t>& outFaceLightmaps)
{
    g_BSP.Reset();
    outFaceLightmaps.clear();

    if (mapData.entities.empty())
    {
        return false;
    }

    InitializeLeaf0();

    Int32 firstFace = 0;
    Int32 numFaces = 0;
    Float mins[3], maxs[3];
    Float origin[3] = { 0.0f, 0.0f, 0.0f };

    EmitModelFaces(mapData.entities[0].brushes, firstFace, numFaces, mins, maxs);
    Int32 worldModelIndex = g_BSP.InsertModel(mins, maxs, origin, firstFace, numFaces);

    std::vector<poly_face_t> worldFaces;
    std::vector<poly_brush_t> worldBrushes;
    for (const auto& brush : mapData.entities[0].brushes)
    {
        poly_brush_t pb;
        if (BuildBrushPolygons(brush, pb))
        {
            worldBrushes.push_back(pb);
            for (const auto& f : pb.faces)
            {
                const dpbspv3texinfo_t& tx = g_BSP.GetTexinfo(f.texinfoIndex);
                const dpbspv3texture_t& tex = g_BSP.GetTexture(tx.miptex);
                if (strcmp(tex.name, "NULL") &&
                    strcmp(tex.name, "CLIP") &&
                    strcmp(tex.name, "SKIP") &&
                    strcmp(tex.name, "HINT") &&
                    strcmp(tex.name, "AAATRIGGER"))
                {
                    worldFaces.push_back(f);
                }
            }
        }
    }
    BuildBSPModelTrees(worldModelIndex, worldFaces, worldBrushes, mins, maxs);

    g_BSP.GetModel(worldModelIndex).visleafs = (Int32)(g_BSP.GetLeafCount() - 1);

    Int32 submodelIdx = 1;
    for (size_t i = 1; i < mapData.entities.size(); i++)
    {
        if (mapData.entities[i].brushes.empty())
        {
            continue;
        }

        EmitModelFaces(mapData.entities[i].brushes, firstFace, numFaces, mins, maxs);
        Int32 subModelIndex = g_BSP.InsertModel(mins, maxs, origin, firstFace, numFaces);

        std::vector<poly_face_t> subFaces;
        std::vector<poly_brush_t> subBrushes;
        for (const auto& brush : mapData.entities[i].brushes)
        {
            poly_brush_t pb;
            if (BuildBrushPolygons(brush, pb))
            {
                subBrushes.push_back(pb);
                for (const auto& f : pb.faces)
                {
                    const dpbspv3texinfo_t& tx = g_BSP.GetTexinfo(f.texinfoIndex);
                    const dpbspv3texture_t& tex = g_BSP.GetTexture(tx.miptex);
                    if (strcmp(tex.name, "NULL") &&
                        strcmp(tex.name, "CLIP") &&
                        strcmp(tex.name, "SKIP") &&
                        strcmp(tex.name, "HINT") &&
                        strcmp(tex.name, "AAATRIGGER"))
                    {
                        subFaces.push_back(f);
                    }
                }
            }
        }

        bool noclip = (atoi(mapData.entities[i].GetValue("zhlt_noclip")) == 1);
        BuildBSPModelTrees(subModelIndex, subFaces, subBrushes, mins, maxs, noclip);

        Char modelName[32];
        snprintf(modelName, sizeof(modelName), "*%d", submodelIdx++);

        map_epair_t ep;
        ep.key = "model";
        ep.value = modelName;
        mapData.entities[i].epairs.push_back(ep);
    }

    g_BSP.ImportDisplacements(dispData);

    size_t faceCount = g_BSP.GetFaceCount();
    for (size_t i = 0; i < faceCount; i++)
    {
        const dpbspv3face_t& bspFace = g_BSP.GetFace(i);
        poly_face_t dummyFace;
        dummyFace.planeIndex = bspFace.planenum;
        dummyFace.texinfoIndex = bspFace.texinfo;

        Int32 firstEdge = bspFace.firstedge;
        Int32 numEdges = bspFace.numedges;
        dummyFace.verts.resize(numEdges);

        for (Int32 e = 0; e < numEdges; e++)
        {
            Int32 surfEdge = g_BSP.GetSurfEdge(firstEdge + e);
            Uint32 vIdx = (surfEdge >= 0) ? g_BSP.GetEdge(surfEdge).vertexes[0] : g_BSP.GetEdge(-surfEdge).vertexes[1];
            const dpbspv3vertex_t& vert = g_BSP.GetVertex(vIdx);
            dummyFace.verts[e].pos[0] = vert.origin[0];
            dummyFace.verts[e].pos[1] = vert.origin[1];
            dummyFace.verts[e].pos[2] = vert.origin[2];
        }

        lightmap_face_t lm;
        CalculateFaceLightmapExtents(dummyFace, (Int32)i, lm);
        outFaceLightmaps.push_back(lm);
    }

    g_BSP.SetEntities(SerializeEntities(mapData));

    return true;
}

std::string SerializeEntities(const map_data_t& mapData)
{
    std::stringstream ss;
    for (const auto& ent : mapData.entities)
    {
        if (ent.epairs.empty())
        {
            continue;
        }

        ss << "{\n";
        for (const auto& ep : ent.epairs)
        {
            if (ep.key.empty() || ep.value.empty())
            {
                continue;
            }
            ss << "\"" << ep.key << "\" \"" << ep.value << "\"\n";
        }
        ss << "}\n";
    }
    return ss.str();
}