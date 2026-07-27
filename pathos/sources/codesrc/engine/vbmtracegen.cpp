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
#include "includes.h"
#include "com_math.h"
#include "system.h"
#include "console.h"
#include "cvar.h"
#include "vbmformat.h"
#include "mcdformat.h"
#include "vbm_shared.h"
#include "vbmtracegen.h"
#include "cache_model.h"
#include "plane.h"
#include <vector>
#include <unordered_map>

CCVar* g_pCvarVBMAutoGenMCD = nullptr;

//=============================================
// @brief Initializes the CVAR for auto-generation
//
//=============================================
void VBM_InitTraceGen( void )
{
	g_pCvarVBMAutoGenMCD = gConsole.CreateCVar(CVAR_FLOAT, FL_CV_SAVE, "vbm_autogen_mcd", "1", "Automatically generates collision meshes (MCD) for VBM models missing them.");
}

//=============================================
// @brief Intermediate structure for building submodel data
//
//=============================================
struct mcd_submodel_data_t 
{
	Vector mins = NULL_MINS;
	Vector maxs = NULL_MAXS;
	std::vector<mcdvertex_t> vertices;
	std::vector<mcdtrimeshtriangle_t> triangles;
	std::vector<mcdbvhnode_t> bvh_nodes;
	std::vector<Int32> bvh_tri_indices;
};

//=============================================
// @brief BVH Builder adapted for vectors
//
//=============================================
struct bvh_builder_t
{
	std::vector<mcdtrimeshtriangle_t>& triangles;
	std::vector<mcdvertex_t>& vertices;
	std::vector<mcdbvhnode_t>& nodes;
	std::vector<Int32>& triindices;

	void build(Uint32 nodeidx, Uint32 starttri, Uint32 endtri, Uint32 depth)
	{
		mcdbvhnode_t& node = nodes[nodeidx];
		node.index = nodeidx;

		Vector nmins = NULL_MINS;
		Vector nmaxs = NULL_MAXS;
		for (Uint32 i = starttri; i < endtri; i++)
		{
			const mcdtrimeshtriangle_t& tri = triangles[triindices[i]];
			for (Uint32 j = 0; j < 3; j++)
			{
				const Vector& v = vertices[tri.trivertexes[j]].origin;
				for (Uint32 k = 0; k < 3; k++)
				{
					if (v[k] < nmins[k]) 
						nmins[k] = v[k];
					if (v[k] > nmaxs[k]) 
						nmaxs[k] = v[k];
				}
			}
		}

		node.mins = nmins - Vector(1.0f, 1.0f, 1.0f);
		node.maxs = nmaxs + Vector(1.0f, 1.0f, 1.0f);

		Uint32 numtris = endtri - starttri;
		if (numtris <= 4 || depth >= 12)
		{
			node.isleaf = true;
			node.numtriangles = numtris;
			node.triindexoffset = starttri; 
			node.children[0] = NO_POSITION;
			node.children[1] = NO_POSITION;
			return;
		}

		node.isleaf = false;
		node.numtriangles = 0;

		Vector size = nmaxs - nmins;
		Int32 axis = 0;
		if (size.y > size.x) 
			axis = 1;
		if (size.z > size[axis]) 
			axis = 2;

		Float splitval = nmins[axis] + size[axis] * 0.5f;

		Uint32 mid = starttri;
		for (Uint32 i = starttri; i < endtri; i++)
		{
			const mcdtrimeshtriangle_t& tri = triangles[triindices[i]];
			Float centroid = (vertices[tri.trivertexes[0]].origin[axis] + vertices[tri.trivertexes[1]].origin[axis] + vertices[tri.trivertexes[2]].origin[axis]) / 3.0f;
			if (centroid < splitval)
			{
				Int32 temp = triindices[i];
				triindices[i] = triindices[mid];
				triindices[mid] = temp;
				mid++;
			}
		}

		if (mid == starttri || mid == endtri)
		{
			for (Uint32 i = starttri; i < endtri; i++)
			{
				for (Uint32 j = i + 1; j < endtri; j++)
				{
					const mcdtrimeshtriangle_t& triI = triangles[triindices[i]];
					const mcdtrimeshtriangle_t& triJ = triangles[triindices[j]];
					Float centroidI = (vertices[triI.trivertexes[0]].origin[axis] + vertices[triI.trivertexes[1]].origin[axis] + vertices[triI.trivertexes[2]].origin[axis]) / 3.0f;
					Float centroidJ = (vertices[triJ.trivertexes[0]].origin[axis] + vertices[triJ.trivertexes[1]].origin[axis] + vertices[triJ.trivertexes[2]].origin[axis]) / 3.0f;
					if (centroidI > centroidJ)
					{
						Int32 temp = triindices[i];
						triindices[i] = triindices[j];
						triindices[j] = temp;
					}
				}
			}
			mid = starttri + numtris / 2;
		}

		Uint32 leftchild = nodes.size();
		nodes.push_back(mcdbvhnode_t());
		
		Uint32 rightchild = nodes.size();
		nodes.push_back(mcdbvhnode_t());

		nodes[nodeidx].children[0] = leftchild;
		nodes[nodeidx].children[1] = rightchild;

		build(leftchild, starttri, mid, depth + 1);
		build(rightchild, mid, endtri, depth + 1);
	}
};

//=============================================
// @brief Generates an in-memory MCD collision block
//
//=============================================
void VBM_GenerateMCD( vbmcache_t* pCache )
{
	if (!pCache || !pCache->pvbmhdr)
		return;

	if (pCache->pmcdheader || (g_pCvarVBMAutoGenMCD && g_pCvarVBMAutoGenMCD->GetValue() <= 0))
		return;

	vbmheader_t* pvbm = pCache->pvbmhdr;
	const vbmvertex_t* pvbmverts = pvbm->getVertexes();
	const Uint32* pvbminds = pvbm->getIndexes();

	std::vector<std::vector<mcd_submodel_data_t>> bodypartsData(pvbm->numbodyparts);

	for (Int32 i = 0; i < pvbm->numbodyparts; i++)
	{
		const vbmbodypart_t* pVbmBody = pvbm->getBodyPart(i);
		bodypartsData[i].resize(pVbmBody->numsubmodels);

		for (Int32 j = 0; j < pVbmBody->numsubmodels; j++)
		{
			const vbmsubmodel_t* pVbmSub = pVbmBody->getSubmodel(pvbm, j);
			mcd_submodel_data_t& subData = bodypartsData[i][j];

			std::unordered_map<Uint32, Uint32> vbmToMcdVertMap;

			for (Int32 k = 0; k < pVbmSub->nummeshes; k++)
			{
				const vbmmesh_t* pMesh = pVbmSub->getMesh(pvbm, k);
				for (Uint32 idx = 0; idx < pMesh->num_indexes; idx += 3)
				{
					Uint32 v0 = pvbminds[pMesh->start_index + idx];
					Uint32 v1 = pvbminds[pMesh->start_index + idx + 1];
					Uint32 v2 = pvbminds[pMesh->start_index + idx + 2];

					Uint32 mcd_v0, mcd_v1, mcd_v2;

					if (vbmToMcdVertMap.find(v0) == vbmToMcdVertMap.end())
					{
						mcd_v0 = subData.vertices.size();
						vbmToMcdVertMap[v0] = mcd_v0;
						mcdvertex_t vert;
						vert.origin = pvbmverts[v0].origin;
						vert.boneindex = NO_POSITION;
						subData.vertices.push_back(vert);
					}
					else
					{
						mcd_v0 = vbmToMcdVertMap[v0];
					}

					if (vbmToMcdVertMap.find(v1) == vbmToMcdVertMap.end())
					{
						mcd_v1 = subData.vertices.size();
						vbmToMcdVertMap[v1] = mcd_v1;
						mcdvertex_t vert;
						vert.origin = pvbmverts[v1].origin;
						vert.boneindex = NO_POSITION;
						subData.vertices.push_back(vert);
					}
					else
					{
						mcd_v1 = vbmToMcdVertMap[v1];
					}

					if (vbmToMcdVertMap.find(v2) == vbmToMcdVertMap.end()) 
					{
						mcd_v2 = subData.vertices.size();
						vbmToMcdVertMap[v2] = mcd_v2;
						mcdvertex_t vert;
						vert.origin = pvbmverts[v2].origin;
						vert.boneindex = NO_POSITION;
						subData.vertices.push_back(vert);
					}
					else
					{
						mcd_v2 = vbmToMcdVertMap[v2];
					}

					mcdtrimeshtriangle_t tri;
					tri.skinref = NO_POSITION;
					tri.trivertexes[0] = mcd_v0;
					tri.trivertexes[1] = mcd_v2;
					tri.trivertexes[2] = mcd_v1;

					Vector e1 = subData.vertices[mcd_v1].origin - subData.vertices[mcd_v0].origin;
					Vector e2 = subData.vertices[mcd_v2].origin - subData.vertices[mcd_v0].origin;
					Math::CrossProduct(e2, e1, tri.normal);
					tri.normal.Normalize();

					tri.distance = Math::DotProduct(tri.normal, subData.vertices[mcd_v0].origin);
					tri.planetype = PLANE_AZ;
					tri.signbits = 0;

					subData.triangles.push_back(tri);

					for (int c = 0; c < 3; c++)
					{
						if (subData.vertices[mcd_v0].origin[c] < subData.mins[c]) 
							subData.mins[c] = subData.vertices[mcd_v0].origin[c];
						if (subData.vertices[mcd_v0].origin[c] > subData.maxs[c]) 
							subData.maxs[c] = subData.vertices[mcd_v0].origin[c];
						
						if (subData.vertices[mcd_v1].origin[c] < subData.mins[c]) 
							subData.mins[c] = subData.vertices[mcd_v1].origin[c];
						if (subData.vertices[mcd_v1].origin[c] > subData.maxs[c]) 
							subData.maxs[c] = subData.vertices[mcd_v1].origin[c];
						
						if (subData.vertices[mcd_v2].origin[c] < subData.mins[c]) 
							subData.mins[c] = subData.vertices[mcd_v2].origin[c];
						if (subData.vertices[mcd_v2].origin[c] > subData.maxs[c]) 
							subData.maxs[c] = subData.vertices[mcd_v2].origin[c];
					}
				}
			}

			Uint32 triCount = subData.triangles.size();
			if (triCount > 0)
			{
				subData.bvh_tri_indices.resize(triCount);
				for (Uint32 t = 0; t < triCount; t++)
					subData.bvh_tri_indices[t] = t;

				bvh_builder_t builder{ subData.triangles, subData.vertices, subData.bvh_nodes, subData.bvh_tri_indices };
				subData.bvh_nodes.push_back(mcdbvhnode_t());
				builder.build(0, 0, triCount, 0);
			}
		}
	}

	Uint32 totalSize = sizeof(mcdheader_t);
	totalSize += pvbm->numbodyparts * sizeof(mcdbodypart_t);
	for (Int32 i = 0; i < pvbm->numbodyparts; i++)
	{
		const vbmbodypart_t* pVbmBody = pvbm->getBodyPart(i);

		for (Int32 j = 0; j < pVbmBody->numsubmodels; j++)
		{
			totalSize += sizeof(mcdsubmodel_t);
			totalSize += 2 * sizeof(mcdcollisiontypemodel_t);
			totalSize += sizeof(mcdtrimeshtype_t);
			totalSize += sizeof(mcdbvhtype_t);

			totalSize += static_cast<Uint32>(bodypartsData[i][j].vertices.size() * sizeof(mcdvertex_t));
			totalSize += static_cast<Uint32>(bodypartsData[i][j].triangles.size() * sizeof(mcdtrimeshtriangle_t));
			totalSize += static_cast<Uint32>(bodypartsData[i][j].bvh_nodes.size() * sizeof(mcdbvhnode_t));
			totalSize += static_cast<Uint32>(bodypartsData[i][j].bvh_tri_indices.size() * sizeof(Int32));
		}
	}

	byte* pBuffer = new byte[totalSize];
	memset(pBuffer, 0, totalSize);

	mcdheader_t* pMCD = reinterpret_cast<mcdheader_t*>(pBuffer);
	pMCD->id = MCD_FORMAT_HEADER;
	pMCD->version = MCD_FORMAT_VERSION;
	pMCD->numbodyparts = pvbm->numbodyparts;
	pMCD->bodypartoffset = sizeof(mcdheader_t);
	pMCD->size = totalSize;

	qstrcpy(pMCD->name, pvbm->name);

	Uint32 curOffset = sizeof(mcdheader_t) + pvbm->numbodyparts * sizeof(mcdbodypart_t);

	for (Int32 i = 0; i < pvbm->numbodyparts; i++)
	{
		const vbmbodypart_t* pVbmBody = pvbm->getBodyPart(i);
		mcdbodypart_t* pMcdBody = reinterpret_cast<mcdbodypart_t*>(pBuffer + pMCD->bodypartoffset + i * sizeof(mcdbodypart_t));

		qstrcpy(pMcdBody->name, pVbmBody->name);
		pMcdBody->numsubmodels = pVbmBody->numsubmodels;
		pMcdBody->base = pVbmBody->base;
		pMcdBody->submodeloffset = curOffset;

		for (Int32 j = 0; j < pVbmBody->numsubmodels; j++)
		{
			const vbmsubmodel_t* pVbmSub = pVbmBody->getSubmodel(pvbm, j);
			mcd_submodel_data_t& subData = bodypartsData[i][j];

			mcdsubmodel_t* pMcdSub = reinterpret_cast<mcdsubmodel_t*>(pBuffer + curOffset);
			curOffset += sizeof(mcdsubmodel_t);
			
			qstrcpy(pMcdSub->name, pVbmSub->name);
			pMcdSub->mins = subData.mins;
			pMcdSub->maxs = subData.maxs;

			pMcdSub->numcollisiontypes = 2;
			pMcdSub->collisiontypesoffset = curOffset;
			
			mcdcollisiontypemodel_t* pCT = reinterpret_cast<mcdcollisiontypemodel_t*>(pBuffer + curOffset);
			curOffset += 2 * sizeof(mcdcollisiontypemodel_t);

			pCT[0].type = MCD_COLLISION_TRIANGLES;
			pCT[0].dataoffset = curOffset;
			mcdtrimeshtype_t* pTriMesh = reinterpret_cast<mcdtrimeshtype_t*>(pBuffer + curOffset);
			curOffset += sizeof(mcdtrimeshtype_t);

			pCT[1].type = MCD_COLLISION_BVH;
			pCT[1].dataoffset = curOffset;
			mcdbvhtype_t* pBVH = reinterpret_cast<mcdbvhtype_t*>(pBuffer + curOffset);
			curOffset += sizeof(mcdbvhtype_t);

			pTriMesh->numvertexes = subData.vertices.size();
			pTriMesh->vertexoffset = curOffset;
			if (pTriMesh->numvertexes > 0)
			{
				memcpy(pBuffer + curOffset, subData.vertices.data(), subData.vertices.size() * sizeof(mcdvertex_t));
				curOffset += subData.vertices.size() * sizeof(mcdvertex_t);
			}

			pTriMesh->numtriangles = subData.triangles.size();
			pTriMesh->triangleoffset = curOffset;
			if (pTriMesh->numtriangles > 0)
			{
				memcpy(pBuffer + curOffset, subData.triangles.data(), subData.triangles.size() * sizeof(mcdtrimeshtriangle_t));
				curOffset += subData.triangles.size() * sizeof(mcdtrimeshtriangle_t);
			}

			pBVH->numbvhnodes = subData.bvh_nodes.size();
			pBVH->bvhnodeoffset = curOffset;
			mcdbvhnode_t* pOutNodes = reinterpret_cast<mcdbvhnode_t*>(pBuffer + curOffset);
			curOffset += subData.bvh_nodes.size() * sizeof(mcdbvhnode_t);

			Uint32 bvhTriIndicesOffset = curOffset;
			if (!subData.bvh_tri_indices.empty())
			{
				memcpy(pBuffer + curOffset, subData.bvh_tri_indices.data(), subData.bvh_tri_indices.size() * sizeof(Int32));
				curOffset += subData.bvh_tri_indices.size() * sizeof(Int32);
			}

			for (Uint32 n = 0; n < pBVH->numbvhnodes; n++)
			{
				pOutNodes[n] = subData.bvh_nodes[n];
				if (pOutNodes[n].isleaf)
				{
					pOutNodes[n].triindexoffset = bvhTriIndicesOffset + (pOutNodes[n].triindexoffset * sizeof(Int32));
				}
			}
		}
	}

	pCache->pmcdheader = pMCD;
}