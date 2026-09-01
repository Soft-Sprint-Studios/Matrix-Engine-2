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
#include "bsp.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

#include <cmath>

CBSPBuilder g_BSP;

CBSPBuilder::CBSPBuilder()
{
    Reset();
}

CBSPBuilder::~CBSPBuilder()
{
}

void CBSPBuilder::Reset()
{
    memset(&m_header, 0, sizeof(m_header));
    m_header.id = MBSP_HEADER;
    m_header.version = MBSPV1_VERSION;
    m_header.flags = MBSPV1_FL_NONE;

    m_fileBuffer.clear();
    m_planes.clear();
    m_vertexes.clear();
    m_edges.clear();

    dmbspv1edge_t dummyEdge;
    dummyEdge.vertexes[0] = 0;
    dummyEdge.vertexes[1] = 0;
    m_edges.push_back(dummyEdge);

    m_surfedges.clear();
    m_textures.clear();
    m_texinfos.clear();
    m_faces.clear();
    m_models.clear();
    m_visibility.clear();
    m_nodes.clear();
    m_clipnodes.clear();
    m_leafs.clear();
    m_marksurfaces.clear();
    m_brushes.clear();
    m_brushsides.clear();
    m_leafbrushes.clear();
    m_entityData.clear();
    m_faceIdToBspIndex.clear();

    for (Int32 i = 0; i < NB_SURF_LIGHTMAP_LAYERS; i++)
    {
        m_lightmaps[i].clear();
    }

    for (Int32 i = 0; i < NB_BAKED_VERTEXLIGHT_LAYERS; i++)
    {
        m_vertexLight[i].clear();
    }

    m_lightGrid.clear();
    m_dispInfos.clear();
    m_dispVerts.clear();
    m_dispFaceMap.clear();
}

Int32 CBSPBuilder::InsertPlane(const Float normal[3], Float distance, Int32 axisType)
{
    for (size_t i = 0; i < m_planes.size(); i++)
    {
        if (m_planes[i].type == axisType &&
            fabsf(m_planes[i].dist - distance) < 0.001f &&
            fabsf(m_planes[i].normal[0] - normal[0]) < 0.0001f &&
            fabsf(m_planes[i].normal[1] - normal[1]) < 0.0001f &&
            fabsf(m_planes[i].normal[2] - normal[2]) < 0.0001f)
        {
            return (Int32)i;
        }
    }

    dmbspv1plane_t pl;
    pl.normal[0] = normal[0];
    pl.normal[1] = normal[1];
    pl.normal[2] = normal[2];
    pl.dist = distance;
    pl.type = axisType;

    m_planes.push_back(pl);
    return (Int32)(m_planes.size() - 1);
}

Int32 CBSPBuilder::InsertVertex(const Float position[3])
{
    for (size_t i = 0; i < m_vertexes.size(); i++)
    {
        if (fabs(m_vertexes[i].origin[0] - position[0]) < 0.005f &&
            fabs(m_vertexes[i].origin[1] - position[1]) < 0.005f &&
            fabs(m_vertexes[i].origin[2] - position[2]) < 0.005f)
        {
            return (Int32)i;
        }
    }

    dmbspv1vertex_t vert;
    vert.origin[0] = position[0];
    vert.origin[1] = position[1];
    vert.origin[2] = position[2];

    m_vertexes.push_back(vert);
    return (Int32)(m_vertexes.size() - 1);
}

Int32 CBSPBuilder::InsertEdge(Uint32 startVertex, Uint32 endVertex)
{
    for (size_t i = 1; i < m_edges.size(); i++)
    {
        if (m_edges[i].vertexes[0] == startVertex && m_edges[i].vertexes[1] == endVertex)
        {
            return (Int32)i;
        }
        if (m_edges[i].vertexes[0] == endVertex && m_edges[i].vertexes[1] == startVertex)
        {
            return -(Int32)i;
        }
    }

    dmbspv1edge_t edge;
    edge.vertexes[0] = startVertex;
    edge.vertexes[1] = endVertex;

    m_edges.push_back(edge);
    return (Int32)(m_edges.size() - 1);
}

Int32 CBSPBuilder::InsertSurfEdge(Int32 edgeIndex)
{
    m_surfedges.push_back(edgeIndex);
    return (Int32)(m_surfedges.size() - 1);
}

Int32 CBSPBuilder::InsertTexture(const Char* name)
{
    for (size_t i = 0; i < m_textures.size(); i++)
    {
        if (!strcmp(m_textures[i].name, name))
        {
            return (Int32)i;
        }
    }

    dmbspv1texture_t tex;
    strncpy(tex.name, name, sizeof(tex.name) - 1);
    tex.name[sizeof(tex.name) - 1] = '\0';

    m_textures.push_back(tex);
    return (Int32)(m_textures.size() - 1);
}

Int32 CBSPBuilder::InsertTexinfo(const Float vecs[2][4], Int32 textureIndex, Int32 flags)
{
    for (size_t i = 0; i < m_texinfos.size(); i++)
    {
        if (m_texinfos[i].miptex != textureIndex || m_texinfos[i].flags != flags)
        {
            continue;
        }

        bool match = true;
        for (Int32 j = 0; j < 2; j++)
        {
            for (Int32 k = 0; k < 4; k++)
            {
                if (fabs(m_texinfos[i].vecs[j][k] - vecs[j][k]) > 0.0001f)
                {
                    match = false;
                    break;
                }
            }
            if (!match)
            {
                break;
            }
        }

        if (match)
        {
            return (Int32)i;
        }
    }

    dmbspv1texinfo_t tx;
    memcpy(tx.vecs, vecs, sizeof(tx.vecs));
    tx.miptex = textureIndex;
    tx.flags = flags;

    m_texinfos.push_back(tx);
    return (Int32)(m_texinfos.size() - 1);
}

Int32 CBSPBuilder::InsertFace(Uint32 planeIndex, Int32 side, Int32 firstEdge, Int32 numEdges, Int32 texinfoIndex, Float sampleScale, Int32 smoothGroup, const byte styles[MBSPV1_MAX_LIGHTMAPS])
{
    dmbspv1face_t f;
    f.planenum = planeIndex;
    f.side = side;
    f.firstedge = firstEdge;
    f.numedges = numEdges;
    f.texinfo = texinfoIndex;
    f.samplescale = (sampleScale <= 0.0f) ? 1.0f : sampleScale;
    f.smoothgroupbits = smoothGroup;
    f.lightoffset = -1;

    if (styles)
    {
        memcpy(f.lmstyles, styles, sizeof(f.lmstyles));
    }
    else
    {
        memset(f.lmstyles, 255, sizeof(f.lmstyles));
        f.lmstyles[0] = 0;
    }

    m_faces.push_back(f);
    return (Int32)(m_faces.size() - 1);
}

Int32 CBSPBuilder::InsertModel(const Float mins[3], const Float maxs[3], const Float origin[3], Int32 firstFace, Int32 numFaces)
{
    dmbspv1model_t mdl;
    for (Int32 i = 0; i < 3; i++)
    {
        mdl.mins[i] = mins[i];
        mdl.maxs[i] = maxs[i];
        mdl.origin[i] = origin[i];
    }
    mdl.firstface = firstFace;
    mdl.numfaces = numFaces;

    m_models.push_back(mdl);
    return (Int32)(m_models.size() - 1);
}

Int32 CBSPBuilder::InsertNode(Int32 planeIndex, Int32 child0, Int32 child1, const Int32 mins[3], const Int32 maxs[3], Uint32 firstFace, Uint32 numFaces)
{
    dmbspv1node_t node;
    node.planenum = planeIndex;
    node.children[0] = child0;
    node.children[1] = child1;
    for (Int32 i = 0; i < 3; i++)
    {
        node.mins[i] = mins[i];
        node.maxs[i] = maxs[i];
    }
    node.firstface = firstFace;
    node.numfaces = numFaces;

    m_nodes.push_back(node);
    return (Int32)(m_nodes.size() - 1);
}

Int32 CBSPBuilder::InsertClipNode(Int32 planeIndex, Int32 child0, Int32 child1)
{
    dmbspv1clipnode_t cn;
    cn.planenum = planeIndex;
    cn.children[0] = child0;
    cn.children[1] = child1;

    m_clipnodes.push_back(cn);
    return (Int32)(m_clipnodes.size() - 1);
}

Int32 CBSPBuilder::InsertLeaf(Int32 contents, Int32 visOffset, const Int32 mins[3], const Int32 maxs[3], Uint32 firstMarkSurface, Uint32 numMarkSurfaces, Uint32 firstLeafBrush, Uint32 numLeafBrushes)
{
    dmbspv1leaf_brush_t leaf;
    leaf.contents = contents;
    leaf.visoffset = visOffset;
    for (Int32 i = 0; i < 3; i++)
    {
        leaf.mins[i] = mins[i];
        leaf.maxs[i] = maxs[i];
    }
    leaf.firstmarksurface = firstMarkSurface;
    leaf.nummarksurfaces = numMarkSurfaces;
    leaf.firstleafbrush = firstLeafBrush;
    leaf.numleafbrushes = numLeafBrushes;

    m_leafs.push_back(leaf);
    return (Int32)(m_leafs.size() - 1);
}

Int32 CBSPBuilder::InsertMarkSurface(Uint32 faceIndex)
{
    m_marksurfaces.push_back(faceIndex);
    return (Int32)(m_marksurfaces.size() - 1);
}

Int32 CBSPBuilder::InsertBrush(Int32 firstSide, Int32 numSides, Int32 contents)
{
    dmbspv1brush_t brush;
    brush.firstside = firstSide;
    brush.numsides = numSides;
    brush.contents = contents;

    m_brushes.push_back(brush);
    return (Int32)(m_brushes.size() - 1);
}

Int32 CBSPBuilder::InsertBrushSide(Int32 planeIndex, Int32 texinfoIndex, Int32 flags)
{
    dmbspv1brushside_t side;
    side.planenum = planeIndex;
    side.texinfo = texinfoIndex;
    side.flags = flags;

    m_brushsides.push_back(side);
    return (Int32)(m_brushsides.size() - 1);
}

Int32 CBSPBuilder::InsertLeafBrush(Uint32 brushIndex)
{
    m_leafbrushes.push_back(brushIndex);
    return (Int32)(m_leafbrushes.size() - 1);
}

void CBSPBuilder::SetVisibilityData(const std::vector<byte>& visData)
{
    m_visibility = visData;
}

void CBSPBuilder::SetEntities(const std::string& entityString)
{
    m_entityData = entityString;
}

void CBSPBuilder::BindFaceId(Int32 mapFaceId, Int32 bspFaceIndex)
{
    m_faceIdToBspIndex.push_back({ mapFaceId, bspFaceIndex });
}

void CBSPBuilder::ImportDisplacements(const map_disp_data_t& dispData)
{
    m_dispInfos.clear();
    m_dispVerts.clear();
    m_dispFaceMap.assign(m_faces.size(), -1);

    for (size_t i = 0; i < dispData.displacements.size(); i++)
    {
        const map_dispinfo_t& src = dispData.displacements[i];

        Int32 resolvedBspFace = -1;
        for (const auto& pair : m_faceIdToBspIndex)
        {
            if (pair.first == src.face_id)
            {
                resolvedBspFace = pair.second;
                break;
            }
        }

        if (resolvedBspFace < 0 || resolvedBspFace >= (Int32)m_faces.size())
        {
            continue;
        }

        dmbspv1dispinfo_t info;
        strncpy(info.texture2, src.texture2, sizeof(info.texture2) - 1);
        info.texture2[sizeof(info.texture2) - 1] = '\0';
        info.face_index = resolvedBspFace;
        info.power = src.power;
        info.vert_start = (Int32)m_dispVerts.size();

        for (Int32 c = 0; c < 4; c++)
        {
            info.corners[c][0] = src.corners[c][0];
            info.corners[c][1] = src.corners[c][1];
            info.corners[c][2] = src.corners[c][2];
        }

        for (size_t v = 0; v < src.verts.size(); v++)
        {
            dmbspv1dispvert_t vert;
            vert.vector[0] = src.verts[v].vector[0];
            vert.vector[1] = src.verts[v].vector[1];
            vert.vector[2] = src.verts[v].vector[2];
            vert.distance = src.verts[v].distance;
            vert.alpha = src.verts[v].alpha;
            m_dispVerts.push_back(vert);
        }

        m_dispFaceMap[resolvedBspFace] = (Int32)m_dispInfos.size();

        m_dispInfos.push_back(info);
    }
}

void CBSPBuilder::SetLightmapLayer(surf_lmap_layers_t layer, const std::vector<byte>& uncompressedData)
{
    if (layer >= 0 && layer < NB_SURF_LIGHTMAP_LAYERS)
    {
        m_lightmaps[layer] = uncompressedData;
    }
}

void CBSPBuilder::SetVertexLightingLayer(baked_vertexlight_layers_t layer, const std::vector<byte>& uncompressedData)
{
    if (layer >= 0 && layer < NB_BAKED_VERTEXLIGHT_LAYERS)
    {
        m_vertexLight[layer] = uncompressedData;
    }
}

void CBSPBuilder::SetLightGridData(const std::vector<byte>& gridData)
{
    m_lightGrid = gridData;
}