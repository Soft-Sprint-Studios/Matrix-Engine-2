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
#include "trace.h"
#include "brushmodel.h"
#include "mcdtrace.h"
#include "mcdformat.h"
#include "disptrace.h"

mcdheader_t* g_pWorldDisplacementMCD = nullptr;

//=============================================
// @brief Builds the dynamic in-memory MCD for displacements
//
//=============================================
void TR_BuildDisplacementMCD( const brushmodel_t& model )
{
	TR_FreeDisplacementMCD();

	if (model.numdispinfo == 0)
		return;

	Uint32 totalVertices = 0;
	Uint32 totalTriangles = 0;
	for (Uint32 i = 0; i < model.numdispinfo; i++)
	{
		const mdispinfo_t& info = model.pdispinfo[i];
		Uint32 side = (1 << info.power) + 1;
		totalVertices += side * side;
		totalTriangles += (side - 1) * (side - 1) * 2;
	}

	if (totalVertices == 0 || totalTriangles == 0)
		return;

	Uint32 headerSize = sizeof(mcdheader_t);
	Uint32 bodypartSize = sizeof(mcdbodypart_t);
	Uint32 submodelSize = sizeof(mcdsubmodel_t);
	Uint32 collisionTypesSize = sizeof(mcdcollisiontypemodel_t) * 2;
	Uint32 trimeshTypeSize = sizeof(mcdtrimeshtype_t);
	Uint32 bvhTypeSize = sizeof(mcdbvhtype_t);

	Uint32 verticesPayloadSize = totalVertices * sizeof(mcdvertex_t);
	Uint32 trianglesPayloadSize = totalTriangles * sizeof(mcdtrimeshtriangle_t);

	Uint32 maxNodes = totalTriangles * 2;
	Uint32 bvhNodesPayloadSize = maxNodes * sizeof(mcdbvhnode_t);
	Uint32 bvhTriIndicesPayloadSize = totalTriangles * sizeof(Int32);

	Uint32 totalBufferSize = headerSize + bodypartSize + submodelSize + collisionTypesSize +
		trimeshTypeSize + bvhTypeSize + verticesPayloadSize + trianglesPayloadSize +
		bvhNodesPayloadSize + bvhTriIndicesPayloadSize;

	byte* pBuffer = new byte[totalBufferSize];
	memset(pBuffer, 0, totalBufferSize);

	mcdheader_t* pHeader = reinterpret_cast<mcdheader_t*>(pBuffer);
	pHeader->id = MCD_FORMAT_HEADER;
	pHeader->version = MCD_FORMAT_VERSION;
	pHeader->numbodyparts = 1;
	pHeader->bodypartoffset = headerSize;

	mcdbodypart_t* pBodyPart = reinterpret_cast<mcdbodypart_t*>(pBuffer + pHeader->bodypartoffset);
	pBodyPart->numsubmodels = 1;
	pBodyPart->submodeloffset = pHeader->bodypartoffset + bodypartSize;
	pBodyPart->base = 1;

	mcdsubmodel_t* pSubModel = reinterpret_cast<mcdsubmodel_t*>(pBuffer + pBodyPart->submodeloffset);
	pSubModel->numcollisiontypes = 2;
	pSubModel->collisiontypesoffset = pBodyPart->submodeloffset + submodelSize;

	mcdcollisiontypemodel_t* pCollTypes = reinterpret_cast<mcdcollisiontypemodel_t*>(pBuffer + pSubModel->collisiontypesoffset);
	pCollTypes[0].type = MCD_COLLISION_TRIANGLES;
	pCollTypes[0].dataoffset = pSubModel->collisiontypesoffset + collisionTypesSize;

	pCollTypes[1].type = MCD_COLLISION_BVH;
	pCollTypes[1].dataoffset = pCollTypes[0].dataoffset + trimeshTypeSize;

	mcdtrimeshtype_t* pTriMesh = reinterpret_cast<mcdtrimeshtype_t*>(pBuffer + pCollTypes[0].dataoffset);
	pTriMesh->numvertexes = totalVertices;
	pTriMesh->vertexoffset = pCollTypes[1].dataoffset + bvhTypeSize;
	pTriMesh->numtriangles = totalTriangles;
	pTriMesh->triangleoffset = pTriMesh->vertexoffset + verticesPayloadSize;

	mcdbvhtype_t* pBVH = reinterpret_cast<mcdbvhtype_t*>(pBuffer + pCollTypes[1].dataoffset);
	pBVH->numbvhnodes = 0;
	pBVH->bvhnodeoffset = pTriMesh->triangleoffset + trianglesPayloadSize;

	mcdvertex_t* pVertices = reinterpret_cast<mcdvertex_t*>(pBuffer + pTriMesh->vertexoffset);
	mcdtrimeshtriangle_t* pTriangles = reinterpret_cast<mcdtrimeshtriangle_t*>(pBuffer + pTriMesh->triangleoffset);
	mcdbvhnode_t* pNodes = reinterpret_cast<mcdbvhnode_t*>(pBuffer + pBVH->bvhnodeoffset);
	Int32* pTriIndices = reinterpret_cast<Int32*>(pBuffer + pBVH->bvhnodeoffset + bvhNodesPayloadSize);

	Uint32 curVertex = 0;
	Uint32 curTriangle = 0;

	Vector mins = NULL_MINS;
	Vector maxs = NULL_MAXS;

	for (Uint32 i = 0; i < model.numdispinfo; i++)
	{
		const mdispinfo_t& info = model.pdispinfo[i];
		Uint32 side = (1 << info.power) + 1;
		Uint32 v_base = curVertex;

		const msurface_t* psurface = &model.psurfaces[info.face_index];
		Vector faceNormal;
		Math::VectorCopy(psurface->pplane->normal, faceNormal);
		if (psurface->flags & SURF_PLANEBACK)
		{
			Math::VectorScale(faceNormal, -1.0f, faceNormal);
		}

		Vector corners[4];
		for (Uint32 c = 0; c < 4; c++)
		{
			corners[c] = Vector(info.corners[c][0], info.corners[c][1], info.corners[c][2]);
		}

		for (Uint32 y = 0; y < side; y++)
		{
			for (Uint32 x = 0; x < side; x++)
			{
				Float fr_x = static_cast<Float>(x) / (side - 1);
				Float fr_y = static_cast<Float>(y) / (side - 1);

				Vector top, bot, pos;
				Math::VectorAdd(corners[0], (corners[1] - corners[0]) * fr_x, top);
				Math::VectorAdd(corners[3], (corners[2] - corners[3]) * fr_x, bot);
				Math::VectorAdd(top, (bot - top) * fr_y, pos);

				Int32 v_idx = info.vert_start + (y * side + x);
				Vector displaced_pos;
				Math::VectorMA(pos, model.pdispverts[v_idx].distance, model.pdispverts[v_idx].vector, displaced_pos);

				mcdvertex_t& v = pVertices[curVertex++];
				v.origin = displaced_pos;
				v.boneindex = NO_POSITION;

				for (Uint32 k = 0; k < 3; k++)
				{
					if (displaced_pos[k] < mins[k]) 
						mins[k] = displaced_pos[k];
					if (displaced_pos[k] > maxs[k]) 
						maxs[k] = displaced_pos[k];
				}
			}
		}

		for (Uint32 y = 0; y < side - 1; y++)
		{
			for (Uint32 x = 0; x < side - 1; x++)
			{
				for (Uint32 tri = 0; tri < 2; tri++)
				{
					mcdtrimeshtriangle_t& t = pTriangles[curTriangle];
					t.skinref = info.face_index;
					if (tri == 0)
					{
						t.trivertexes[0] = v_base + (y * side + x);
						t.trivertexes[1] = v_base + ((y + 1) * side + (x + 1));
						t.trivertexes[2] = v_base + ((y + 1) * side + x);
					}
					else
					{
						t.trivertexes[0] = v_base + (y * side + x);
						t.trivertexes[1] = v_base + (y * side + (x + 1));
						t.trivertexes[2] = v_base + ((y + 1) * side + (x + 1));
					}

					Vector e1 = pVertices[t.trivertexes[1]].origin - pVertices[t.trivertexes[0]].origin;
					Vector e2 = pVertices[t.trivertexes[2]].origin - pVertices[t.trivertexes[0]].origin;
					Math::CrossProduct(e1, e2, t.normal);
					t.normal.Normalize();

					if (Math::DotProduct(t.normal, faceNormal) < 0.0f)
					{
						t.normal = -t.normal;
						Uint32 temp = t.trivertexes[1];
						t.trivertexes[1] = t.trivertexes[2];
						t.trivertexes[2] = temp;
					}

					t.distance = Math::DotProduct(t.normal, pVertices[t.trivertexes[0]].origin);
					t.planetype = PLANE_AZ;
					t.signbits = 0;

					pTriIndices[curTriangle] = curTriangle;
					curTriangle++;
				}
			}
		}
	}

	pSubModel->mins = mins;
	pSubModel->maxs = maxs;

	struct bvh_builder_t
	{
		const byte* pBaseBuffer;
		const mcdtrimeshtriangle_t* ptriangles;
		const mcdvertex_t* pvertices;
		mcdbvhnode_t* pnodes;
		Int32* ptriindices;
		Uint32 numn_nodes;

		void build(Uint32 nodeidx, Uint32 starttri, Uint32 endtri, Uint32 depth)
		{
			mcdbvhnode_t& node = pnodes[nodeidx];
			node.index = nodeidx;

			Vector nmins = NULL_MINS;
			Vector nmaxs = NULL_MAXS;
			for (Uint32 i = starttri; i < endtri; i++)
			{
				const mcdtrimeshtriangle_t& tri = ptriangles[ptriindices[i]];
				for (Uint32 j = 0; j < 3; j++)
				{
					const Vector& v = pvertices[tri.trivertexes[j]].origin;
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
				node.triindexoffset = (reinterpret_cast<byte*>(ptriindices) - pBaseBuffer) + starttri * sizeof(Int32);
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
				const mcdtrimeshtriangle_t& tri = ptriangles[ptriindices[i]];
				Float centroid = (pvertices[tri.trivertexes[0]].origin[axis] + pvertices[tri.trivertexes[1]].origin[axis] + pvertices[tri.trivertexes[2]].origin[axis]) / 3.0f;
				if (centroid < splitval)
				{
					Int32 temp = ptriindices[i];
					ptriindices[i] = ptriindices[mid];
					ptriindices[mid] = temp;
					mid++;
				}
			}

			if (mid == starttri || mid == endtri)
			{
				for (Uint32 i = starttri; i < endtri; i++)
				{
					for (Uint32 j = i + 1; j < endtri; j++)
					{
						const mcdtrimeshtriangle_t& triI = ptriangles[ptriindices[i]];
						const mcdtrimeshtriangle_t& triJ = ptriangles[ptriindices[j]];
						Float centroidI = (pvertices[triI.trivertexes[0]].origin[axis] + pvertices[triI.trivertexes[1]].origin[axis] + pvertices[triI.trivertexes[2]].origin[axis]) / 3.0f;
						Float centroidJ = (pvertices[triJ.trivertexes[0]].origin[axis] + pvertices[triJ.trivertexes[1]].origin[axis] + pvertices[triJ.trivertexes[2]].origin[axis]) / 3.0f;
						if (centroidI > centroidJ)
						{
							Int32 temp = ptriindices[i];
							ptriindices[i] = ptriindices[j];
							ptriindices[j] = temp;
						}
					}
				}
				mid = starttri + numtris / 2;
			}

			Uint32 leftchild = numn_nodes++;
			Uint32 rightchild = numn_nodes++;
			node.children[0] = leftchild;
			node.children[1] = rightchild;

			build(leftchild, starttri, mid, depth + 1);
			build(rightchild, mid, endtri, depth + 1);
		}
	};

	bvh_builder_t builder;
	builder.pBaseBuffer = pBuffer;
	builder.ptriangles = pTriangles;
	builder.pvertices = pVertices;
	builder.pnodes = pNodes;
	builder.ptriindices = pTriIndices;
	builder.numn_nodes = 1;

	builder.build(0, 0, totalTriangles, 0);

	pBVH->numbvhnodes = builder.numn_nodes;
	pHeader->size = totalBufferSize;

	g_pWorldDisplacementMCD = pHeader;
}

//=============================================
// @brief Clears the dynamic displacement MCD
//
//=============================================
void TR_FreeDisplacementMCD( void )
{
	if (g_pWorldDisplacementMCD)
	{
		delete[] reinterpret_cast<byte*>(g_pWorldDisplacementMCD);
		g_pWorldDisplacementMCD = nullptr;
	}
}

//=============================================
// @brief Checks line/AABB intersections against displacements
//
//=============================================
bool TR_TraceDisplacementMCD( const Vector& start, const Vector& end, const Vector& mins, const Vector& maxs, trace_t& trace )
{
	if (!g_pWorldDisplacementMCD)
		return false;

	trace_t tr;
	if (mins.IsZero() && maxs.IsZero())
	{
		g_mcdTrace.TraceLinePoint(start, end, g_pWorldDisplacementMCD, 0, tr);
	}
	else
	{
		g_mcdTrace.TraceLineAABB(start, end, mins, maxs, g_pWorldDisplacementMCD, 0, tr);
	}

	if (tr.fraction < trace.fraction || tr.allSolid() || tr.startSolid())
	{
		trace = tr;
		trace.hitentity = WORLDSPAWN_ENTITY_INDEX;
		return true;
	}

	return false;
}