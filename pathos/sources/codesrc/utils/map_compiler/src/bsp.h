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
#ifndef BSP_H
#define BSP_H

#include "datatypes.h"
#include "pbspv3file.h"
#include "aldformat.h"
#include "mapparser.h"
#include <vector>
#include <string>

#define PBSP_ID (('P' << 24) + ('S' << 16) + ('B' << 8) + 'P')

class CBSPBuilder
{
public:
    CBSPBuilder();
    ~CBSPBuilder();

    void Reset();

    Int32 InsertPlane(const Float normal[3], Float distance, Int32 axisType);
    Int32 InsertVertex(const Float position[3]);
    Int32 InsertEdge(Uint32 startVertex, Uint32 endVertex);
    Int32 InsertSurfEdge(Int32 edgeIndex);
    Int32 InsertTexture(const Char* name);
    Int32 InsertTexinfo(const Float vecs[2][4], Int32 textureIndex, Int32 flags);
    Int32 InsertFace(Uint32 planeIndex, Int32 side, Int32 firstEdge, Int32 numEdges, Int32 texinfoIndex, Float sampleScale, Int32 smoothGroup, const byte styles[PBSPV3_MAX_LIGHTMAPS]);
    Int32 InsertModel(const Float mins[3], const Float maxs[3], const Float origin[3], Int32 firstFace, Int32 numFaces);
    Int32 InsertNode(Int32 planeIndex, Int32 child0, Int32 child1, const Int32 mins[3], const Int32 maxs[3], Uint32 firstFace, Uint32 numFaces);
    Int32 InsertClipNode(Int32 planeIndex, Int32 child0, Int32 child1);
    Int32 InsertLeaf(Int32 contents, Int32 visOffset, const Int32 mins[3], const Int32 maxs[3], Uint32 firstMarkSurface, Uint32 numMarkSurfaces, Uint32 firstLeafBrush, Uint32 numLeafBrushes);
    Int32 InsertMarkSurface(Uint32 faceIndex);
    Int32 InsertBrush(Int32 firstSide, Int32 numSides, Int32 contents);
    Int32 InsertBrushSide(Int32 planeIndex, Int32 texinfoIndex, Int32 flags);
    Int32 InsertLeafBrush(Uint32 brushIndex);
    void SetVisibilityData(const std::vector<byte>& visData);

    void SetEntities(const std::string& entityString);
    void BindFaceId(Int32 mapFaceId, Int32 bspFaceIndex);
    void ImportDisplacements(const map_disp_data_t& dispData);
    void SetLightmapLayer(surf_lmap_layers_t layer, const std::vector<byte>& uncompressedData);
    void SetVertexLightingLayer(baked_vertexlight_layers_t layer, const std::vector<byte>& uncompressedData);
    void SetLightGridData(const std::vector<byte>& gridData);

    bool ExportFile(const Char* filename);
    bool ExportALD(const Char* filename, aldlumptype_t lumpType);

    size_t GetFaceCount() const;
    size_t GetModelCount() const;
    size_t GetSurfEdgeCount() const;
    size_t GetMarkSurfaceCount() const;
    size_t GetLeafBrushCount() const;
    size_t GetBrushSideCount() const;

    const dpbspv3model_t& GetModel(size_t index) const;
    dpbspv3model_t& GetModel(size_t index);
    size_t GetLeafCount() const;
    size_t GetNodeCount() const;
    const dpbspv3node_t& GetNode(size_t index) const;
    dpbspv3node_t& GetNode(size_t index);
    const dpbspv3leaf_brush_t& GetLeaf(size_t index) const;
    dpbspv3leaf_brush_t& GetLeaf(size_t index);
    const dpbspv3face_t& GetFace(size_t index) const;
    dpbspv3face_t& GetFace(size_t index);
    const dpbspv3texinfo_t& GetTexinfo(size_t index) const;
    const dpbspv3plane_t& GetPlane(size_t index) const;
    const dpbspv3vertex_t& GetVertex(size_t index) const;
    const dpbspv3edge_t& GetEdge(size_t index) const;
    Int32 GetSurfEdge(size_t index) const;
    const dpbspv3texture_t& GetTexture(size_t index) const;
    Int32 ResolveFaceId(Int32 faceId) const;
    Int32 GetFaceDispIndex(size_t faceIndex) const;
    const dpbspv3dispinfo_t& GetDispInfo(size_t index) const;
    const dpbspv3dispvert_t& GetDispVert(size_t index) const;

    void SetFaceLightOffset(Int32 faceIndex, Int32 offset);

private:
    void AppendLumpData(Int32 lumpIndex, const void* data, size_t size);
    void AppendLightingLump(Int32 lumpIndex, const std::vector<byte>& data);
    Uint64 ComputeChecksum(const byte* buffer, size_t size) const;

    dpbspv3header_t m_header;
    std::vector<byte> m_fileBuffer;

    std::vector<dpbspv3plane_t> m_planes;
    std::vector<dpbspv3vertex_t> m_vertexes;
    std::vector<dpbspv3edge_t> m_edges;
    std::vector<Int32> m_surfedges;
    std::vector<dpbspv3texture_t> m_textures;
    std::vector<dpbspv3texinfo_t> m_texinfos;
    std::vector<dpbspv3face_t> m_faces;
    std::vector<dpbspv3model_t> m_models;
    std::vector<byte> m_visibility;
    std::vector<dpbspv3node_t> m_nodes;
    std::vector<dpbspv3clipnode_t> m_clipnodes;
    std::vector<dpbspv3leaf_brush_t> m_leafs;
    std::vector<Uint32> m_marksurfaces;
    std::vector<dpbspv3brush_t> m_brushes;
    std::vector<dpbspv3brushside_t> m_brushsides;
    std::vector<Uint32> m_leafbrushes;
    std::string m_entityData;

    std::vector<byte> m_lightmaps[NB_SURF_LIGHTMAP_LAYERS];
    std::vector<byte> m_vertexLight[NB_BAKED_VERTEXLIGHT_LAYERS];
    std::vector<byte> m_lightGrid;

    std::vector<dpbspv3dispinfo_t> m_dispInfos;
    std::vector<dpbspv3dispvert_t> m_dispVerts;
    std::vector<Int32> m_dispFaceMap;
    std::vector<std::pair<Int32, Int32>> m_faceIdToBspIndex;
};

extern CBSPBuilder g_BSP;

#endif // BSP_H