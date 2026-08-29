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
#ifndef VBM_H
#define VBM_H

#include "datatypes.h"
#include <vector>
#include <string>

#define VBM_HEADER_ID (('1' << 24) + ('M' << 16) + ('B' << 8) + 'V')

struct vbm_vertex_t
{
    Float origin[3];
    Float normal[3];
    Float tangent[4];
    Float texcoord[2];
    Int16 flexvertindex;
    byte boneindexes[4];
    byte boneweights[4];
};

struct vbm_boneinfo_t
{
    Char name[64];
    Int32 flags;
    Int32 index;
    Int32 parentindex;
    Float position[3];
    Float angles[3];
    Float scale[6];
    Float bindtransform[3][4];
};

struct vbm_mesh_t
{
    Int32 boneoffset;
    Int32 numbones;
    Int32 skinref;
    Int32 start_index;
    Int32 num_indexes;
};

struct vbm_submodel_t
{
    Char name[32];
    Int32 meshoffset;
    Int32 nummeshes;
    Int32 flexinfoindex;
    Int32 lodoffset;
    Int32 numlods;
};

struct vbm_bodypart_t
{
    Char name[32];
    Int32 base;
    Int32 numsubmodels;
    Int32 submodeloffset;
};

struct vbm_header_t
{
    Int32 id;
    Char name[128];
    Int32 flags;
    Int32 vertexoffset;
    Int32 numverts;
    Int32 indexoffset;
    Int32 numindexes;
    Int32 bodypartoffset;
    Int32 numbodyparts;
    Int32 textureoffset;
    Int32 numtextures;
    Int32 numflexinfo;
    Int32 flexinfooffset;
    Int32 numflexcontrollers;
    Int32 flexcontrolleroffset;
    Int32 controlleroffset;
    Int32 numcontrollers;
    Int32 boneinfooffset;
    Int32 numboneinfo;
    Int32 skinoffset;
    Int32 numskinref;
    Int32 numskinfamilies;
    Int32 size;
    Int32 ibooffset;
    Int32 vbooffset;
};

struct vbm_model_t
{
    std::string filename;
    std::string vertexHash;
    Int32 numVerts;
    std::vector<vbm_vertex_t> rawVerts;
    std::vector<Uint32> indexes;
    std::vector<vbm_bodypart_t> bodyparts;
    std::vector<vbm_submodel_t> submodels;
    std::vector<vbm_mesh_t> meshes;
    std::vector<vbm_boneinfo_t> bones;

    std::vector<Float> worldVerts;
    std::vector<Float> worldNormals;
    std::vector<Uint32> sceneIndices;
};

bool LoadVBMModel(const Char* filename, const Float origin[3], const Float angles[3], Float scale, vbm_model_t& outModel);

#endif // VBM_H