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
#ifndef PBSPV3FILE_H
#define PBSPV3FILE_H

#include "contents.h"

//
// BSP limits
//

static constexpr Uint32 PBSPV3_MAX_MAP_HULLS			= 4;
static constexpr Uint32 PBSPV3_MAX_MAP_MODELS			= 4096;
static constexpr Uint32 PBSPV3_MAX_MAP_BRUSHES			= 262144;
static constexpr Uint32 PBSPV3_MAX_MAP_ENTITIES			= 65535;
static constexpr Uint32 PBSPV3_MAX_MAP_ENTSTRING		= 2097152;
static constexpr Uint32 PBSPV3_MAX_MAP_PLANES			= 262144;
static constexpr Uint32 PBSPV3_MAX_MAP_NODES			= 262144;
static constexpr Uint32 PBSPV3_MAX_MAP_CLIPNODES		= 262144;
static constexpr Uint32 PBSPV3_MAX_MAP_LEAFS			= 262144;
static constexpr Uint32 PBSPV3_MAX_MAP_VERTS			= 262144;
static constexpr Uint32 PBSPV3_MAX_MAP_FACES			= 262144;
static constexpr Uint32 PBSPV3_MAX_MAP_MARKSURFACES		= 262144;
static constexpr Uint32 PBSPV3_MAX_MAP_TEXINFO			= 262144;
static constexpr Uint32 PBSPV3_MAX_MAP_EDGES			= 524288;
static constexpr Uint32 PBSPV3_MAX_MAP_SURFEDGES		= 1048576;
static constexpr Uint32 PBSPV3_MAX_MAP_TEXTURES			= 16384;
static constexpr Uint32 PBSPV3_MAX_TEXTURE_ANIMS		= 10;
static constexpr Uint32 PBSPV3_MAX_MAP_LIGHTING			= 16777216;
static constexpr Uint32 PBSPV3_MAX_MAP_VISIBILITY		= 16777216;

static constexpr Uint32 PBSPV3_MAX_LIGHTMAPS			= 4;
static constexpr Uint32 PBSPV3_LM_SAMPLE_SIZE			= 16;
static constexpr Uint32 PBSPV3_NUM_AMBIENTS				= 4;
static constexpr Uint32 PBSPV3_VERSION					= 3;
static constexpr Uint32 PBSPV3_MAX_SURFACE_EXTENTS		= 2048;

//
// BSP lumps
//
enum pbspv3_lumps_t
{
	PBSPV3_LUMP_ENTITIES = 0,
	PBSPV3_LUMP_PLANES,
	PBSPV3_LUMP_TEXTURES,
	PBSPV3_LUMP_VERTEXES,
	PBSPV3_LUMP_VISIBILITY,
	PBSPV3_LUMP_NODES,
	PBSPV3_LUMP_TEXINFO,
	PBSPV3_LUMP_FACES,
	PBSPV3_LUMP_LIGHTING_DEFAULT,
	PBSPV3_LUMP_LIGHTING_AMBIENT,
	PBSPV3_LUMP_LIGHTING_DIFFUSE,
	PBSPV3_LUMP_LIGHTING_VECTORS,
	PBSPV3_LUMP_CLIPNODES,
	PBSPV3_LUMP_LEAFS,
	PBSPV3_LUMP_MARKSURFACES,
	PBSPV3_LUMP_EDGES,
	PBSPV3_LUMP_SURFEDGES,
	PBSPV3_LUMP_MODELS,
	PBSPV3_LUMP_VERTEX_LIGHTING_AMBIENT,
	PBSPV3_LUMP_VERTEX_LIGHTING_DIFFUSE,
	PBSPV3_LUMP_VERTEX_LIGHTING_VECTORS,
	PBSPV3_LUMP_LIGHTGRID_DATA,
    PBSPV3_LUMP_BRUSHES,
    PBSPV3_LUMP_BRUSHSIDES,
    PBSPV3_LUMP_LEAFBRUSHES,
	PBSPV3_LUMP_DISPLACEMENTS,
	PBSPV3_LUMP_CHECKSUM,

	// MUST BE LAST
	PBSPV3_NB_LUMPS // Don't actually use this anywhere if possible
};

//
// Flags for BSP file
//
enum pbspv3_flags_t
{
	PBSPV3_FL_NONE						= 0
};

//
// Flags for brush side
//
enum pbspv3_brushside_flags_t
{
	PBSPV3_BSIDE_FL_PLANEBACK			= (1<<0),
	PBSPV3_BSIDE_FL_BEVEL				= (1<<1)
};

//
// Header for Pathos BSP V3
//

struct dpbspv3lump_t
{
	dpbspv3lump_t():
		offset(0),
		size(0)
	{}

	Int32 offset;
	Int32 size;
};

struct dpbspv3header_t
{
	dpbspv3header_t():
		id(0),
		version(0),
		flags(0)
	{
		memset(lumps, 0, sizeof(lumps));
	}

	Int32 id;
	Int32 version;
	Int64 flags;

	dpbspv3lump_t lumps[PBSPV3_NB_LUMPS];
};

//
// BSP file structures
//
struct dpbspv3model_t
{
	dpbspv3model_t():
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

	Int32 headnode[PBSPV3_MAX_MAP_HULLS];
	Int32 visleafs;

	Int32 firstface;
	Int32 numfaces;
};

struct dpbspv3vertex_t
{
	dpbspv3vertex_t()
	{
		memset(origin, 0, sizeof(origin));
	}

	Float origin[3];
};

struct dpbspv3plane_t
{
	dpbspv3plane_t():
		dist(0),
		type(0)
	{
		memset(normal, 0, sizeof(normal));
	}

	Float normal[3];
	Float dist;

	Int32 type;
};

struct dpbspv3node_t
{
	dpbspv3node_t():
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

struct dpbspv3clipnode_t
{
	dpbspv3clipnode_t():
		planenum(0)
	{
		memset(children, 0, sizeof(children));
	}

	Int32 planenum;
	Int32 children[2];
};

struct dpbspv3texinfo_t
{
	dpbspv3texinfo_t():
		miptex(0),
		flags(0)
	{
		memset(vecs, 0, sizeof(vecs));
	}

	Float vecs[2][4];
	Int32 miptex;
	Int32 flags;
};

struct dpbspv3edge_t
{
	Uint32 vertexes[2];
};

struct dpbspv3face_t
{
	dpbspv3face_t():
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
	Int32 smoothgroupbits; // This is set if pheader->flags has PBSPV3_FL_HAS_SMOOTHING_GROUPS set

	byte lmstyles[PBSPV3_MAX_LIGHTMAPS];
	Int32 lightoffset;
};

struct dpbspv3leaf_nobrush_t
{
	dpbspv3leaf_nobrush_t():
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

	byte ambient_level[PBSPV3_NUM_AMBIENTS];
};

struct dpbspv3leaf_brush_t
{
	dpbspv3leaf_brush_t():
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

struct dpbspv3lightingdata_t
{
	dpbspv3lightingdata_t():
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

struct dpbspv3lightgridlumpheader_t
{
    dpbspv3lightgridlumpheader_t():
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

struct dpbspv3lightgridnode_t
{
    dpbspv3lightgridnode_t()
    {
        for(Uint32 i = 0; i < 3; i++)
			divisionpoint[i] = 0;

         for(Uint32 i = 0; i < 8; i++)
			children[i] = 0;
    }

    Int32 divisionpoint[3];
    Int32 children[8];
};

struct dpbspv3lightgridleaf_t
{
    dpbspv3lightgridleaf_t():
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

struct dpbspv3lightgridsample_t
{
    dpbspv3lightgridsample_t():
        rawsampleoffset(-1)
    {
        memset(styles, 0, sizeof(styles));
    }

	byte styles[PBSPV3_MAX_LIGHTMAPS];
    Int32 rawsampleoffset;
};

struct dpbspv3brushside_t
{
    dpbspv3brushside_t():
        planenum(0),
        texinfo(0),
		flags(0)
    {}

    Int32 planenum;
    Int32 texinfo;
	Int32 flags;
};

struct dpbspv3brush_t
{
    dpbspv3brush_t():
        firstside(0),
        numsides(0),
        contents(0)
    {}

    Int32 firstside;
    Int32 numsides;
    Int32 contents;
};

struct dpbspv3texture_t
{
	Char name[64];
};

struct dpbspv3dispheader_t
{
	Int32 num_disp_infos;
	Int32 num_disp_verts;
	Int32 num_faces;
};

struct dpbspv3dispinfo_t
{
	Char texture2[64];
	Int32 face_index;
	Int32 power;
	Int32 vert_start;
	Float corners[4][3];
};

struct dpbspv3dispvert_t
{
	Float vector[3];
	Float distance;
	Float alpha;
};

struct dpbspv3checksum_t
{
	Uint64 checksum;
};

#endif //PBSPV3FILE_H