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
#include "bsp_tree.h"
#include "brush.h"
#include "bsp.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <algorithm>

static constexpr Int32 MAX_BSP_TREE_DEPTH = 64;

struct bsp_build_face_t
{
    Int32 planeIndex;
    Int32 texinfoIndex;
    Int32 face_id;
    Float normal[3];
    Float dist;
    std::vector<poly_vert_t> verts;
};

static void CalculateBounds(const std::vector<bsp_build_face_t>& faces, Int32 mins[3], Int32 maxs[3])
{
    mins[0] = mins[1] = mins[2] = 999999;
    maxs[0] = maxs[1] = maxs[2] = -999999;

    for (const auto& f : faces)
    {
        for (const auto& v : f.verts)
        {
            for (Int32 k = 0; k < 3; k++)
            {
                Int32 minVal = (Int32)floorf(v.pos[k]) - 16;
                Int32 maxVal = (Int32)ceilf(v.pos[k]) + 16;
                if (minVal < mins[k])
                    mins[k] = minVal;
                if (maxVal > maxs[k])
                    maxs[k] = maxVal;
            }
        }
    }
}

static Int32 FindBestSplitPlane(const std::vector<bsp_build_face_t>& faces)
{
    Int32 bestIdx = -1;
    Int32 bestScore = 99999999;

    for (size_t i = 0; i < faces.size(); i++)
    {
        const auto& candidate = faces[i];
        Int32 frontCount = 0, backCount = 0, splitCount = 0;

        for (size_t j = 0; j < faces.size(); j++)
        {
            if (i == j)
                continue;

            const auto& f = faces[j];

            Int32 onFront = 0, onBack = 0;
            for (const auto& v : f.verts)
            {
                Float d = (v.pos[0] * candidate.normal[0] + v.pos[1] * candidate.normal[1] + v.pos[2] * candidate.normal[2]) - candidate.dist;
                if (d > 0.04f)
                    onFront++;
                else if (d < -0.04f)
                    onBack++;
            }

            if (onFront && onBack)
                splitCount++;
            else if (onFront)
                frontCount++;
            else if (onBack)
                backCount++;
        }

        Int32 score = abs(frontCount - backCount) + splitCount * 2;
        if (score < bestScore)
        {
            bestScore = score;
            bestIdx = (Int32)i;
        }
    }

    return (bestIdx >= 0) ? faces[bestIdx].planeIndex : -1;
}

static Int32 EmitFaceToBSP(const bsp_build_face_t& face, Int32 nodePlaneIndex)
{
    size_t vertCount = face.verts.size();
    std::vector<Uint32> vertIndices(vertCount);

    for (size_t v = 0; v < vertCount; v++)
    {
        vertIndices[v] = (Uint32)g_BSP.InsertVertex(face.verts[v].pos);
    }

    Int32 firstSurfEdge = (Int32)g_BSP.GetSurfEdgeCount();
    for (size_t v = 0; v < vertCount; v++)
    {
        Uint32 v1 = vertIndices[v];
        Uint32 v2 = vertIndices[(v + 1) % vertCount];

        Int32 edgeIndex = g_BSP.InsertEdge(v1, v2);
        g_BSP.InsertSurfEdge(edgeIndex);
    }

    const dpbspv3texinfo_t& tx = g_BSP.GetTexinfo(face.texinfoIndex);
    Float lenU = sqrtf(tx.vecs[0][0] * tx.vecs[0][0] + tx.vecs[0][1] * tx.vecs[0][1] + tx.vecs[0][2] * tx.vecs[0][2]);
    Float lenV = sqrtf(tx.vecs[1][0] * tx.vecs[1][0] + tx.vecs[1][1] * tx.vecs[1][1] + tx.vecs[1][2] * tx.vecs[1][2]);
    Float avgLen = (lenU + lenV) * 0.5f;
    Float faceScale = (avgLen > 0.0f) ? (1.0f / avgLen) : 1.0f;

    byte styles[PBSPV3_MAX_LIGHTMAPS];
    memset(styles, 255, sizeof(styles));
    styles[0] = 0;

    const dpbspv3plane_t& nPlane = g_BSP.GetPlane(nodePlaneIndex);
    Float dotN = face.normal[0] * nPlane.normal[0] + face.normal[1] * nPlane.normal[1] + face.normal[2] * nPlane.normal[2];
    Int32 side = (dotN < 0.0f) ? 1 : 0;

    Int32 bspFaceIndex = g_BSP.InsertFace(nodePlaneIndex, side, firstSurfEdge, (Int32)vertCount, face.texinfoIndex, faceScale, 0, styles);
    if (face.face_id >= 0)
    {
        g_BSP.BindFaceId(face.face_id, bspFaceIndex);
    }
    return bspFaceIndex;
}

static Int32 PartitionAndEmitTree(std::vector<bsp_build_face_t>& faces, const std::vector<Uint32>& modelBrushIndices, std::vector<Int32>& modelEmittedFaces, Int32& outVisLeafCount, Int32 outNodeMins[3], Int32 outNodeMaxs[3], Int32 currentDepth = 0)
{
    auto CreateLeaf = [&](Int32 contents) -> Int32 {
        Int32 mins[3] = { 999999, 999999, 999999 };
        Int32 maxs[3] = { -999999, -999999, -999999 };

        if (!faces.empty())
        {
            CalculateBounds(faces, mins, maxs);
        }
        else
        {
            mins[0] = mins[1] = mins[2] = 0;
            maxs[0] = maxs[1] = maxs[2] = 0;
        }

        for (Int32 k = 0; k < 3; k++)
        {
            outNodeMins[k] = mins[k];
            outNodeMaxs[k] = maxs[k];
        }

        if (contents == CONTENTS_SOLID)
        {
            return -1;
        }

        Uint32 firstLBrush = (Uint32)g_BSP.GetLeafBrushCount();
        for (Uint32 lb : modelBrushIndices)
        {
            g_BSP.InsertLeafBrush(lb);
        }

        Int32 leafIndex = g_BSP.InsertLeaf(contents, 0, mins, maxs, 0, 0, firstLBrush, (Uint32)modelBrushIndices.size());
        outVisLeafCount++;
        return -(leafIndex + 1);
        };

    if (faces.empty())
    {
        return CreateLeaf(CONTENTS_EMPTY);
    }

    Int32 splitPlane = FindBestSplitPlane(faces);
    if (splitPlane < 0)
    {
        splitPlane = faces[0].planeIndex;
    }

    const dpbspv3plane_t& pl = g_BSP.GetPlane(splitPlane);
    Float normal[3] = { pl.normal[0], pl.normal[1], pl.normal[2] };
    Float dist = pl.dist;

    std::vector<bsp_build_face_t> frontFaces;
    std::vector<bsp_build_face_t> backFaces;
    std::vector<bsp_build_face_t> coplanarFaces;

    for (const auto& f : faces)
    {
        const dpbspv3plane_t& fPl = g_BSP.GetPlane(f.planeIndex);
        Float dot = fPl.normal[0] * normal[0] + fPl.normal[1] * normal[1] + fPl.normal[2] * normal[2];
        bool isSamePlane = (fabsf(dot - 1.0f) < 0.001f && fabsf(fPl.dist - dist) < 0.04f);
        bool isOppositePlane = (fabsf(dot + 1.0f) < 0.001f && fabsf(fPl.dist + dist) < 0.04f);

        if (isSamePlane || isOppositePlane)
        {
            coplanarFaces.push_back(f);
            continue;
        }

        Float sumDist = 0.0f;
        for (const auto& v : f.verts)
        {
            sumDist += (v.pos[0] * normal[0] + v.pos[1] * normal[1] + v.pos[2] * normal[2]) - dist;
        }

        if (sumDist >= 0.0f)
        {
            frontFaces.push_back(f);
        }
        else
        {
            backFaces.push_back(f);
        }
    }

    Uint32 firstFace = (Uint32)g_BSP.GetFaceCount();
    Uint32 numFaces = (Uint32)coplanarFaces.size();

    for (const auto& cf : coplanarFaces)
    {
        Int32 newFaceIdx = EmitFaceToBSP(cf, splitPlane);
        modelEmittedFaces.push_back(newFaceIdx);
    }

    Int32 nodeMins[3] = { 999999, 999999, 999999 };
    Int32 nodeMaxs[3] = { -999999, -999999, -999999 };
    CalculateBounds(faces, nodeMins, nodeMaxs);

    Int32 nodeIndex = g_BSP.InsertNode(splitPlane, -1, -1, nodeMins, nodeMaxs, firstFace, numFaces);

    Int32 c0Mins[3] = { 999999, 999999, 999999 };
    Int32 c0Maxs[3] = { -999999, -999999, -999999 };
    Int32 c1Mins[3] = { 999999, 999999, 999999 };
    Int32 c1Maxs[3] = { -999999, -999999, -999999 };

    Int32 child0 = frontFaces.empty() ? CreateLeaf(CONTENTS_EMPTY) : PartitionAndEmitTree(frontFaces, modelBrushIndices, modelEmittedFaces, outVisLeafCount, c0Mins, c0Maxs, currentDepth + 1);
    Int32 child1 = backFaces.empty() ? CreateLeaf(CONTENTS_EMPTY) : PartitionAndEmitTree(backFaces, modelBrushIndices, modelEmittedFaces, outVisLeafCount, c1Mins, c1Maxs, currentDepth + 1);

    dpbspv3node_t& node = g_BSP.GetNode(nodeIndex);
    node.children[0] = child0;
    node.children[1] = child1;

    for (Int32 k = 0; k < 3; k++)
    {
        Int32 combinedMin = std::min({ nodeMins[k], c0Mins[k], c1Mins[k] });
        Int32 combinedMax = std::max({ nodeMaxs[k], c0Maxs[k], c1Maxs[k] });

        if (combinedMin > combinedMax)
        {
            combinedMin = nodeMins[k];
            combinedMax = nodeMaxs[k];
        }

        node.mins[k] = combinedMin;
        node.maxs[k] = combinedMax;
        outNodeMins[k] = combinedMin;
        outNodeMaxs[k] = combinedMax;
    }

    return nodeIndex;
}

static Int32 EmitClipTreeRecursive(std::vector<bsp_build_face_t>& faces, Int32 currentDepth = 0)
{
    if (faces.empty() || currentDepth >= MAX_BSP_TREE_DEPTH)
    {
        return CONTENTS_EMPTY;
    }

    Int32 splitPlane = FindBestSplitPlane(faces);
    if (splitPlane < 0)
    {
        splitPlane = faces[0].planeIndex;
    }

    const dpbspv3plane_t& pl = g_BSP.GetPlane(splitPlane);
    Float normal[3] = { pl.normal[0], pl.normal[1], pl.normal[2] };
    Float dist = pl.dist;

    std::vector<bsp_build_face_t> frontFaces;
    std::vector<bsp_build_face_t> backFaces;

    for (const auto& f : faces)
    {
        const dpbspv3plane_t& fPl = g_BSP.GetPlane(f.planeIndex);
        Float dot = fPl.normal[0] * normal[0] + fPl.normal[1] * normal[1] + fPl.normal[2] * normal[2];
        Float distDiff = fabsf(fPl.dist - dist);

        if ((fabsf(dot - 1.0f) < 0.001f || fabsf(dot + 1.0f) < 0.001f) && distDiff < 0.04f)
        {
            continue;
        }

        Float sumDist = 0.0f;
        for (const auto& v : f.verts)
        {
            sumDist += (v.pos[0] * normal[0] + v.pos[1] * normal[1] + v.pos[2] * normal[2]) - dist;
        }

        if (sumDist >= 0.0f)
        {
            frontFaces.push_back(f);
        }
        else
        {
            backFaces.push_back(f);
        }
    }

    Int32 c0 = frontFaces.empty() ? CONTENTS_EMPTY : EmitClipTreeRecursive(frontFaces, currentDepth + 1);
    Int32 c1 = backFaces.empty() ? CONTENTS_SOLID : EmitClipTreeRecursive(backFaces, currentDepth + 1);

    return g_BSP.InsertClipNode(splitPlane, c0, c1);
}

bool BuildBSPModelTrees(Int32 modelIndex, const std::vector<poly_face_t>& modelFaces, const std::vector<poly_brush_t>& modelBrushes, const Float mins[3], const Float maxs[3], bool skipClip)
{
    std::vector<Uint32> brushIndices;
    for (const auto& b : modelBrushes)
    {
        if (b.faces.empty())
            continue;

        Int32 firstSide = -1;
        for (const auto& f : b.faces)
        {
            if (f.planeIndex < 0 || f.texinfoIndex < 0)
                continue;

            Int32 sideIdx = g_BSP.InsertBrushSide(f.planeIndex, f.texinfoIndex, 0);
            if (firstSide < 0)
                firstSide = sideIdx;
        }

        if (firstSide < 0)
            continue;

        Int32 numSides = (Int32)g_BSP.GetBrushSideCount() - firstSide;
        if (numSides < 4)
            continue;

        Int32 bIdx = g_BSP.InsertBrush(firstSide, numSides, CONTENTS_SOLID);
        brushIndices.push_back((Uint32)bIdx);
    }

    std::vector<bsp_build_face_t> buildFaces;
    for (size_t i = 0; i < modelFaces.size(); i++)
    {
        bsp_build_face_t bf;
        bf.planeIndex = modelFaces[i].planeIndex;
        bf.texinfoIndex = modelFaces[i].texinfoIndex;
        bf.face_id = modelFaces[i].face_id;
        bf.normal[0] = modelFaces[i].normal[0];
        bf.normal[1] = modelFaces[i].normal[1];
        bf.normal[2] = modelFaces[i].normal[2];
        bf.dist = modelFaces[i].dist;
        bf.verts = modelFaces[i].verts;
        buildFaces.push_back(bf);
    }

    Int32 startFaceCount = (Int32)g_BSP.GetFaceCount();
    std::vector<Int32> modelEmittedFaces;
    Int32 visLeafCount = 0;
    size_t startLeafCount = g_BSP.GetLeafCount();

    Int32 modelRootMins[3] = { (Int32)floorf(mins[0]) - 64, (Int32)floorf(mins[1]) - 64, (Int32)floorf(mins[2]) - 64 };
    Int32 modelRootMaxs[3] = { (Int32)ceilf(maxs[0]) + 64,  (Int32)ceilf(maxs[1]) + 64,  (Int32)ceilf(maxs[2]) + 64 };

    Int32 rootNode = PartitionAndEmitTree(buildFaces, brushIndices, modelEmittedFaces, visLeafCount, modelRootMins, modelRootMaxs, 0);
    Int32 endFaceCount = (Int32)g_BSP.GetFaceCount();
    Int32 totalEmittedFaces = endFaceCount - startFaceCount;

    for (size_t lIdx = startLeafCount; lIdx < g_BSP.GetLeafCount(); lIdx++)
    {
        auto& leaf = g_BSP.GetLeaf(lIdx);
        if (leaf.contents != CONTENTS_SOLID)
        {
            leaf.firstmarksurface = (Uint32)g_BSP.GetMarkSurfaceCount();
            leaf.nummarksurfaces = (Uint32)totalEmittedFaces;
            for (Int32 fIdx = startFaceCount; fIdx < endFaceCount; fIdx++)
            {
                g_BSP.InsertMarkSurface((Uint32)fIdx);
            }
        }
    }

    Int32 clipHead = skipClip ? -1 : EmitClipTreeRecursive(buildFaces, 0);

    if (modelIndex >= 0 && modelIndex < (Int32)g_BSP.GetModelCount())
    {
        dpbspv3model_t& mdl = g_BSP.GetModel(modelIndex);
        mdl.headnode[0] = rootNode;
        mdl.headnode[1] = clipHead;
        mdl.headnode[2] = clipHead;
        mdl.headnode[3] = clipHead;
        mdl.visleafs = visLeafCount;
        mdl.firstface = startFaceCount;
        mdl.numfaces = totalEmittedFaces;

        for (Int32 k = 0; k < 3; k++)
        {
            mdl.mins[k] = mins[k];
            mdl.maxs[k] = maxs[k];
        }
    }

    return true;
}