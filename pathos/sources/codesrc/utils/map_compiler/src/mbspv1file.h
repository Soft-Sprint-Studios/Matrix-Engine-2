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
#ifndef MBSPV1FILE_H
#define MBSPV1FILE_H

#include "contents.h"

//
// BSP limits
//

static constexpr Uint32 MBSPV1_MAX_MAP_HULLS			= 4;
static constexpr Uint32 MBSPV1_MAX_TEXTURE_ANIMS		= 10;
static constexpr Uint32 MBSPV1_MAX_LIGHTMAPS			= 4;
static constexpr Uint32 MBSPV1_LM_SAMPLE_SIZE			= 16;
static constexpr Uint32 MBSPV1_NUM_AMBIENTS				= 4;
static constexpr Uint32 MBSPV1_VERSION					= 1;
static constexpr Uint32 MBSPV1_MAX_SURFACE_EXTENTS		= 2048;

//
// BSP lumps
//
enum mbspv1_lumps_t
{
	MBSPV1_LUMP_ENTITIES = 0,
	MBSPV1_LUMP_PLANES,
	MBSPV1_LUMP_TEXTURES,
	MBSPV1_LUMP_VERTEXES,
	MBSPV1_LUMP_VISIBILITY,
	MBSPV1_LUMP_NODES,
	MBSPV1_LUMP_TEXINFO,
	MBSPV1_LUMP_FACES,
	MBSPV1_LUMP_LIGHTING_DEFAULT,
	MBSPV1_LUMP_LIGHTING_AMBIENT,
	MBSPV1_LUMP_LIGHTING_DIFFUSE,
	MBSPV1_LUMP_LIGHTING_VECTORS,
	MBSPV1_LUMP_CLIPNODES,
	MBSPV1_LUMP_LEAFS,
	MBSPV1_LUMP_MARKSURFACES,
	MBSPV1_LUMP_EDGES,
	MBSPV1_LUMP_SURFEDGES,
	MBSPV1_LUMP_MODELS,
	MBSPV1_LUMP_VERTEX_LIGHTING_AMBIENT,
	MBSPV1_LUMP_VERTEX_LIGHTING_DIFFUSE,
	MBSPV1_LUMP_VERTEX_LIGHTING_VECTORS,
	MBSPV1_LUMP_LIGHTGRID_DATA,
    MBSPV1_LUMP_BRUSHES,
    MBSPV1_LUMP_BRUSHSIDES,
    MBSPV1_LUMP_LEAFBRUSHES,
	MBSPV1_LUMP_DISPLACEMENTS,
	MBSPV1_LUMP_CHECKSUM,

	// MUST BE LAST
	MBSPV1_NB_LUMPS // Don't actually use this anywhere if possible
};

//
// Flags for BSP file
//
enum mbspv1_flags_t
{
	MBSPV1_FL_NONE						= 0
};

//
// Flags for brush side
//
enum mbspv1_brushside_flags_t
{
	MBSPV1_BSIDE_FL_PLANEBACK			= (1<<0),
	MBSPV1_BSIDE_FL_BEVEL				= (1<<1)
};

//
// Header for Pathos BSP V1
//

struct dmbspv1lump_t
{
	dmbspv1lump_t():
		offset(0),
		size(0)
	{}

	Int32 offset;
	Int32 size;
};

struct dmbspv1header_t
{
	dmbspv1header_t():
		id(0),
		version(0),
		flags(0)
	{
		memset(lumps, 0, sizeof(lumps));
	}

	Int32 id;
	Int32 version;
	Int64 flags;

	dmbspv1lump_t lumps[MBSPV1_NB_LUMPS];
};

//
// BSP file structures
//
struct dmbspv1model_t
{
	dmbspv1model_t():
		visleafs(0),
		firstface(0),
		numfaces(0)
	{
		memset(mins, 0, sizeof(mins));
		memset(maxs, 0, sizeof(maxs));
		memset(origin, 0, sizeof(origin));
		memset(headnode, 0, sizeof(headnode));
	}

	Float mins[3];
	Float maxs[3];
	Float origin[3];

	Int32 headnode[MBSPV1_MAX_MAP_HULLS];
	Int32 visleafs;

	Int32 firstface;
	Int32 numfaces;
};

struct dmbspv1vertex_t
{
	dmbspv1vertex_t()
	{
		memset(origin, 0, sizeof(origin));
	}

	Float origin[3];
};

struct dmbspv1plane_t
{
	dmbspv1plane_t():
		dist(0),
		type(0)
	{
		memset(normal, 0, sizeof(normal));
	}

	Float normal[3];
	Float dist;

	Int32 type;
};

struct dmbspv1node_t
{
	dmbspv1node_t():
		planenum(0),
		firstface(0),
		numfaces(0)
	{
		memset(children, 0, sizeof(children));
		memset(mins, 0, sizeof(mins));
		memset(maxs, 0, sizeof(maxs));
	}

	Int32 planenum;
	Int32 children[2];
	Int32 mins[3];
	Int32 maxs[3];

	Uint32 firstface;
	Uint32 numfaces;
};

struct dmbspv1clipnode_t
{
	dmbspv1clipnode_t():
		planenum(0)
	{
		memset(children, 0, sizeof(children));
	}

	Int32 planenum;
	Int32 children[2];
};

struct dmbspv1texinfo_t
{
	dmbspv1texinfo_t():
		miptex(0),
		flags(0)
	{
		memset(vecs, 0, sizeof(vecs));
	}

	Float vecs[2][4];
	Int32 miptex;
	Int32 flags;
};

struct dmbspv1edge_t
{
	Uint32 vertexes[2];
};

struct dmbspv1face_t
{
	dmbspv1face_t():
		planenum(0),
		side(0),
		firstedge(0),
		numedges(0),
		texinfo(0),
		samplescale(0),
		smoothgroupbits(0),
		lightoffset(0)
	{
		memset(lmstyles, 0, sizeof(lmstyles));
	}

	Uint32 planenum;
	Int32 side;

	Int32 firstedge;
	Int32 numedges;
	Int32 texinfo;
	Float samplescale;
	Int32 smoothgroupbits; // This is set if pheader->flags has MBSPV1_FL_HAS_SMOOTHING_GROUPS set

	byte lmstyles[MBSPV1_MAX_LIGHTMAPS];
	Int32 lightoffset;
};

struct dmbspv1leaf_nobrush_t
{
	dmbspv1leaf_nobrush_t():
		contents(0),
		visoffset(0),
		firstmarksurface(0),
		nummarksurfaces(0)
	{
		memset(mins, 0, sizeof(mins));
		memset(maxs, 0, sizeof(maxs));
		memset(ambient_level, 0, sizeof(ambient_level));
	}

	Int32 contents;
	Int32 visoffset;

	Int32 mins[3];
	Int32 maxs[3];

	Uint32 firstmarksurface;
	Uint32 nummarksurfaces;

	byte ambient_level[MBSPV1_NUM_AMBIENTS];
};

struct dmbspv1leaf_brush_t
{
	dmbspv1leaf_brush_t():
		contents(0),
		visoffset(0),
		firstmarksurface(0),
		nummarksurfaces(0),
		firstleafbrush(0),
		numleafbrushes(0)
	{
		memset(mins, 0, sizeof(mins));
		memset(maxs, 0, sizeof(maxs));
	}

	Int32 contents;
	Int32 visoffset;

	Int32 mins[3];
	Int32 maxs[3];

	Uint32 firstmarksurface;
	Uint32 nummarksurfaces;

	Uint32 firstleafbrush;
	Uint32 numleafbrushes;
};

struct dmbspv1lightingdata_t
{
	dmbspv1lightingdata_t():
		compression(0),
		compressionlevel(0),
		dataoffset(0),
		datasize(0),
		noncompressedsize(0)
	{}

	Int32 compression;
	Int32 compressionlevel;
	Int32 dataoffset;
	Int32 datasize;
	Int32 noncompressedsize;
};

struct dmbspv1lightgridlumpheader_t
{
    dmbspv1lightgridlumpheader_t():
        rootnodeindex(-1),
		totalsize(0),
        leafsoffset(-1),
        numleafs(0),
        nodesoffset(-1),
        numnodes(0),
        sampleoffset(-1),
        numsamples(0),
        rawsampledatasize(0),
        ambientdataoffset(-1),
        ambientcompressedsize(0),
        ambientcompressionlevel(0),
        ambientcompressiontype(0),
        diffusedataoffset(-1),
        diffusecompressedsize(0),
        diffusecompressionlevel(0),
        diffusecompressiontype(0),
        vectorsdataoffset(-1),
        vectorscompressedsize(0),
        vectorscompressionlevel(0),
        vectorscompressiontype(0)
    {
        for(Uint32 i = 0; i < 3; i++)
			grid_distance[i] = 0;

        for(Uint32 i = 0; i < 3; i++)
			grid_size[i] = 0;
    }

    Int32 grid_distance[3];
    Int32 grid_size[3];
    Float grid_mins[3];
    Int32 rootnodeindex;
    Uint32 totalsize;

    Int32 leafsoffset;
    Int32 numleafs;

    Int32 nodesoffset;
    Int32 numnodes;

    Int32 sampleoffset;
    Int32 numsamples;

	Int32 rawsampledatasize;

    Int32 ambientdataoffset;
    Int32 ambientcompressedsize;
    Int32 ambientcompressionlevel;
    Int32 ambientcompressiontype;

    Int32 diffusedataoffset;
    Int32 diffusecompressedsize;
    Int32 diffusecompressionlevel;
    Int32 diffusecompressiontype;

    Int32 vectorsdataoffset;
    Int32 vectorscompressedsize;
    Int32 vectorscompressionlevel;
    Int32 vectorscompressiontype;
};

struct dmbspv1lightgridnode_t
{
    dmbspv1lightgridnode_t()
    {
        for(Uint32 i = 0; i < 3; i++)
			divisionpoint[i] = 0;

         for(Uint32 i = 0; i < 8; i++)
			children[i] = 0;
    }

    Int32 divisionpoint[3];
    Int32 children[8];
};

struct dmbspv1lightgridleaf_t
{
    dmbspv1lightgridleaf_t():
        firstsample(-1),
        numsamples(0)
    {
        for(Uint32 i = 0; i < 3; i++)
			mins[i] = 0;

        for(Uint32 i = 0; i < 3; i++)
			size[i] = 0;
    }

	Int32 mins[3];
	Int32 size[3];
    
    Int32 firstsample;
    Int32 numsamples;
};

struct dmbspv1lightgridsample_t
{
    dmbspv1lightgridsample_t():
        rawsampleoffset(-1)
    {
        memset(styles, 0, sizeof(styles));
    }

	byte styles[MBSPV1_MAX_LIGHTMAPS];
    Int32 rawsampleoffset;
};

struct dmbspv1brushside_t
{
    dmbspv1brushside_t():
        planenum(0),
        texinfo(0),
		flags(0)
    {}

    Int32 planenum;
    Int32 texinfo;
	Int32 flags;
};

struct dmbspv1brush_t
{
    dmbspv1brush_t():
        firstside(0),
        numsides(0),
        contents(0)
    {}

    Int32 firstside;
    Int32 numsides;
    Int32 contents;
};

struct dmbspv1texture_t
{
	Char name[64];
};

struct dmbspv1dispheader_t
{
	Int32 num_disp_infos;
	Int32 num_disp_verts;
	Int32 num_faces;
};

struct dmbspv1dispinfo_t
{
	Char texture2[64];
	Int32 face_index;
	Int32 power;
	Int32 vert_start;
	Float corners[4][3];
};

struct dmbspv1dispvert_t
{
	Float vector[3];
	Float distance;
	Float alpha;
};

struct dmbspv1checksum_t
{
	Uint64 checksum;
};

#endif //MBSPV1FILE_H