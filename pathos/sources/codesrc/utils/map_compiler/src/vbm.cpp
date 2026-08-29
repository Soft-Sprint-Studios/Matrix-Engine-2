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
#include "vbm.h"
#include "md5.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>

struct vbm_mat3x4_t
{
    Float m[3][4];
};

static void AngleMatrix( const Float angles[3], Float matrix[3][4] )
{
    Float angle = angles[1] * (M_PI * 2.0f / 360.0f);
    Float sy = sinf(angle);
    Float cy = cosf(angle);

    angle = angles[0] * (M_PI * 2.0f / 360.0f);
    Float sp = sinf(angle);
    Float cp = cosf(angle);

    angle = angles[2] * (M_PI * 2.0f / 360.0f);
    Float sr = sinf(angle);
    Float cr = cosf(angle);

    matrix[0][0] = cp * cy;
    matrix[1][0] = cp * sy;
    matrix[2][0] = -sp;

    matrix[0][1] = sr * sp * cy + cr * -sy;
    matrix[1][1] = sr * sp * sy + cr * cy;
    matrix[2][1] = sr * cp;

    matrix[0][2] = (cr * sp * cy + -sr * -sy);
    matrix[1][2] = (cr * sp * sy + -sr * cy);
    matrix[2][2] = cr * cp;

    matrix[0][3] = 0.0f;
    matrix[1][3] = 0.0f;
    matrix[2][3] = 0.0f;
}

static void ConcatTransforms( const Float in1[3][4], const Float in2[3][4], Float out[3][4] )
{
    out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] + in1[0][2] * in2[2][0];
    out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] + in1[0][2] * in2[2][1];
    out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] + in1[0][2] * in2[2][2];
    out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] + in1[0][2] * in2[2][3] + in1[0][3];

    out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] + in1[1][2] * in2[2][0];
    out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] + in1[1][2] * in2[2][1];
    out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] + in1[1][2] * in2[2][2];
    out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] + in1[1][2] * in2[2][3] + in1[1][3];

    out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] + in1[2][2] * in2[2][0];
    out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] + in1[2][2] * in2[2][1];
    out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] + in1[2][2] * in2[2][2];
    out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] + in1[2][2] * in2[2][3] + in1[2][3];
}

bool LoadVBMModel(const Char* filename, const Float origin[3], const Float angles[3], Float scale, vbm_model_t& outModel)
{
    FILE* f = fopen(filename, "rb");
    if (!f)
        return false;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size < (long)sizeof(vbm_header_t))
    {
        fclose(f);
        return false;
    }

    std::vector<byte> buf(size);
    fread(buf.data(), 1, size, f);
    fclose(f);

    const vbm_header_t* hdr = reinterpret_cast<const vbm_header_t*>(buf.data());
    if (hdr->id != VBM_HEADER_ID)
        return false;

    outModel.filename = filename;
    outModel.numVerts = hdr->numverts;
    outModel.rawVerts.resize(hdr->numverts);
    memcpy(outModel.rawVerts.data(), buf.data() + hdr->vertexoffset, hdr->numverts * sizeof(vbm_vertex_t));

    CMD5 md5(reinterpret_cast<const byte*>(outModel.rawVerts.data()), hdr->numverts * sizeof(vbm_vertex_t));
    outModel.vertexHash = md5.HexDigest();

    outModel.indexes.resize(hdr->numindexes);
 
    memcpy(outModel.indexes.data(), buf.data() + hdr->indexoffset, hdr->numindexes * sizeof(Uint32));

    outModel.bones.resize(hdr->numboneinfo);
    if (hdr->numboneinfo > 0)
        memcpy(outModel.bones.data(), buf.data() + hdr->boneinfooffset, hdr->numboneinfo * sizeof(vbm_boneinfo_t));

    Float entityMatrix[3][4];
    Float invAngles[3] = { -angles[0], angles[1], angles[2] };
    AngleMatrix(invAngles, entityMatrix);

    Float currentScale = (scale <= 0.0f) ? 1.0f : scale;
    for (int r = 0; r < 3; r++)
    {
        for (int c = 0; c < 3; c++)
            entityMatrix[r][c] *= currentScale;
        entityMatrix[r][3] = origin[r];
    }

    outModel.worldVerts.resize(hdr->numverts * 3);
    outModel.worldNormals.resize(hdr->numverts * 3);
    outModel.sceneIndices.clear();

    Float bmin[3] = { 999999.0f, 999999.0f, 999999.0f };
    Float bmax[3] = { -999999.0f, -999999.0f, -999999.0f };

    for (Int32 i = 0; i < hdr->numverts; i++)
    {
        const vbm_vertex_t& v = outModel.rawVerts[i];

        Float finalPos[3];
        finalPos[0] = v.origin[0] * entityMatrix[0][0] + v.origin[1] * entityMatrix[0][1] + v.origin[2] * entityMatrix[0][2] + entityMatrix[0][3];
        finalPos[1] = v.origin[0] * entityMatrix[1][0] + v.origin[1] * entityMatrix[1][1] + v.origin[2] * entityMatrix[1][2] + entityMatrix[1][3];
        finalPos[2] = v.origin[0] * entityMatrix[2][0] + v.origin[1] * entityMatrix[2][1] + v.origin[2] * entityMatrix[2][2] + entityMatrix[2][3];

        Float finalNorm[3];
        finalNorm[0] = v.normal[0] * entityMatrix[0][0] + v.normal[1] * entityMatrix[0][1] + v.normal[2] * entityMatrix[0][2];
        finalNorm[1] = v.normal[0] * entityMatrix[1][0] + v.normal[1] * entityMatrix[1][1] + v.normal[2] * entityMatrix[1][2];
        finalNorm[2] = v.normal[0] * entityMatrix[2][0] + v.normal[1] * entityMatrix[2][1] + v.normal[2] * entityMatrix[2][2];

        Float len = sqrtf(finalNorm[0] * finalNorm[0] + finalNorm[1] * finalNorm[1] + finalNorm[2] * finalNorm[2]);
        if (len > 0.0001f)
        {
            finalNorm[0] /= len;
            finalNorm[1] /= len;
            finalNorm[2] /= len;
        }

        outModel.worldVerts[i * 3 + 0] = finalPos[0];
        outModel.worldVerts[i * 3 + 1] = finalPos[1];
        outModel.worldVerts[i * 3 + 2] = finalPos[2];

        outModel.worldNormals[i * 3 + 0] = finalNorm[0];
        outModel.worldNormals[i * 3 + 1] = finalNorm[1];
        outModel.worldNormals[i * 3 + 2] = finalNorm[2];

        for (Int32 k = 0; k < 3; k++)
        {
            if (finalPos[k] < bmin[k]) 
                bmin[k] = finalPos[k];
            if (finalPos[k] > bmax[k]) 
                bmax[k] = finalPos[k];
        }
    }

    const vbm_bodypart_t* pBodyparts = reinterpret_cast<const vbm_bodypart_t*>(buf.data() + hdr->bodypartoffset);
    for (Int32 bp = 0; bp < hdr->numbodyparts; bp++)
    {
        if (pBodyparts[bp].numsubmodels <= 0)
            continue;

        const vbm_submodel_t* pSubmodels = reinterpret_cast<const vbm_submodel_t*>(buf.data() + pBodyparts[bp].submodeloffset);
        const vbm_submodel_t& sub = pSubmodels[0];

        const vbm_mesh_t* pMeshes = reinterpret_cast<const vbm_mesh_t*>(buf.data() + sub.meshoffset);
        for (Int32 m = 0; m < sub.nummeshes; m++)
        {
            const vbm_mesh_t& mesh = pMeshes[m];
            for (Int32 idx = 0; idx < mesh.num_indexes; idx += 3)
            {
                outModel.sceneIndices.push_back(outModel.indexes[mesh.start_index + idx + 0]);
                outModel.sceneIndices.push_back(outModel.indexes[mesh.start_index + idx + 2]);
                outModel.sceneIndices.push_back(outModel.indexes[mesh.start_index + idx + 1]);
            }
        }
    }
    return true;
}