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
#include <cstring>

//=============================================
// @brief
//
//=============================================
size_t CBSPBuilder::GetFaceCount() const
{
    return m_faces.size();
}

//=============================================
// @brief
//
//=============================================
size_t CBSPBuilder::GetModelCount() const
{
    return m_models.size();
}

//=============================================
// @brief
//
//=============================================
const dmbspv1model_t& CBSPBuilder::GetModel(size_t index) const
{
    return m_models[index];
}

//=============================================
// @brief
//
//=============================================
dmbspv1model_t& CBSPBuilder::GetModel(size_t index)
{
    return m_models[index];
}

//=============================================
// @brief
//
//=============================================
size_t CBSPBuilder::GetLeafCount() const
{
    return m_leafs.size();
}

//=============================================
// @brief
//
//=============================================
size_t CBSPBuilder::GetNodeCount() const
{
    return m_nodes.size();
}

//=============================================
// @brief
//
//=============================================
const dmbspv1node_t& CBSPBuilder::GetNode(size_t index) const
{
    return m_nodes[index];
}

//=============================================
// @brief
//
//=============================================
dmbspv1node_t& CBSPBuilder::GetNode(size_t index)
{
    return m_nodes[index];
}

//=============================================
// @brief
//
//=============================================
const dmbspv1leaf_brush_t& CBSPBuilder::GetLeaf(size_t index) const
{
    return m_leafs[index];
}

//=============================================
// @brief
//
//=============================================
dmbspv1leaf_brush_t& CBSPBuilder::GetLeaf(size_t index)
{
    return m_leafs[index];
}

//=============================================
// @brief
//
//=============================================
size_t CBSPBuilder::GetSurfEdgeCount() const
{
    return m_surfedges.size();
}

//=============================================
// @brief
//
//=============================================
size_t CBSPBuilder::GetMarkSurfaceCount() const
{
    return m_marksurfaces.size();
}

//=============================================
// @brief
//
//=============================================
size_t CBSPBuilder::GetLeafBrushCount() const
{
    return m_leafbrushes.size();
}

//=============================================
// @brief
//
//=============================================
size_t CBSPBuilder::GetBrushSideCount() const
{
    return m_brushsides.size();
}

//=============================================
// @brief
//
//=============================================
const dmbspv1face_t& CBSPBuilder::GetFace(size_t index) const
{
    return m_faces[index];
}

//=============================================
// @brief
//
//=============================================
dmbspv1face_t& CBSPBuilder::GetFace(size_t index)
{
    return m_faces[index];
}

//=============================================
// @brief
//
//=============================================
const dmbspv1texinfo_t& CBSPBuilder::GetTexinfo(size_t index) const
{
    return m_texinfos[index];
}

//=============================================
// @brief
//
//=============================================
const dmbspv1plane_t& CBSPBuilder::GetPlane(size_t index) const
{
    return m_planes[index];
}

//=============================================
// @brief
//
//=============================================
const dmbspv1vertex_t& CBSPBuilder::GetVertex(size_t index) const
{
    return m_vertexes[index];
}

//=============================================
// @brief
//
//=============================================
const dmbspv1edge_t& CBSPBuilder::GetEdge(size_t index) const
{
    return m_edges[index];
}

//=============================================
// @brief
//
//=============================================
Int32 CBSPBuilder::GetSurfEdge(size_t index) const
{
    return m_surfedges[index];
}

//=============================================
// @brief
//
//=============================================
const dmbspv1texture_t& CBSPBuilder::GetTexture(size_t index) const
{
    return m_textures[index];
}

//=============================================
// @brief
//
//=============================================
Int32 CBSPBuilder::ResolveFaceId(Int32 faceId) const
{
    for (const auto& pair : m_faceIdToBspIndex)
    {
        if (pair.first == faceId)
            return pair.second;
    }
    return -1;
}

//=============================================
// @brief
//
//=============================================
Int32 CBSPBuilder::GetFaceDispIndex(size_t faceIndex) const
{
    return m_dispFaceMap[faceIndex];
}

//=============================================
// @brief
//
//=============================================
const dmbspv1dispinfo_t& CBSPBuilder::GetDispInfo(size_t index) const
{
    return m_dispInfos[index];
}

//=============================================
// @brief
//
//=============================================
const dmbspv1dispvert_t& CBSPBuilder::GetDispVert(size_t index) const
{
    return m_dispVerts[index];
}

//=============================================
// @brief
//
//=============================================
void CBSPBuilder::SetFaceLightOffset(Int32 faceIndex, Int32 offset)
{
    if (faceIndex >= 0 && faceIndex < (Int32)m_faces.size())
    {
        m_faces[faceIndex].lightoffset = offset;
    }
}