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
#include "file.h"
#include "texturemanager.h"
#include "brushmodel.h"
#include "pbspv3file.h"
#include "pbspv3.h"
#include "system.h"
#include "logfile.h"
#include "enginestate.h"
#include "bsp_shared.h"
#include "miniz.h"

//=============================================
// @brief
//
//=============================================
brushmodel_t* PBSPV3_Load( const byte* pfile, const dpbspv3header_t* pheader, const Char* pstrFilename )
{
	// Create the brushmodel_t object
	brushmodel_t* pmodel = new brushmodel_t();
	
	// Set the version info
	pmodel->name = pstrFilename;
	pmodel->version = pheader->version;
	pmodel->freedata = true;

	// Load all lumps
	if (!PBSPV3_LoadChecksum(pfile, (*pmodel), pheader->lumps[PBSPV3_LUMP_CHECKSUM])
		|| !PBSPV3_LoadVertexes(pfile, (*pmodel), pheader->lumps[PBSPV3_LUMP_VERTEXES])
		|| !PBSPV3_LoadEdges(pfile, (*pmodel), pheader->lumps[PBSPV3_LUMP_EDGES])
		|| !PBSPV3_LoadSurfedges(pfile, (*pmodel), pheader->lumps[PBSPV3_LUMP_SURFEDGES])
		|| !PBSPV3_LoadTextures(pfile, (*pmodel), pheader->lumps[PBSPV3_LUMP_TEXTURES])
		|| !PBSPV3_LoadDefaultLighting(pfile, (*pmodel), pheader->lumps[PBSPV3_LUMP_LIGHTING_DEFAULT])
		|| !PBSPV3_LoadLightingDataLayer(pfile, (*pmodel), pheader->lumps[PBSPV3_LUMP_LIGHTING_AMBIENT], SURF_LIGHTMAP_AMBIENT)
		|| !PBSPV3_LoadLightingDataLayer(pfile, (*pmodel), pheader->lumps[PBSPV3_LUMP_LIGHTING_DIFFUSE], SURF_LIGHTMAP_DIFFUSE)
		|| !PBSPV3_LoadLightingDataLayer(pfile, (*pmodel), pheader->lumps[PBSPV3_LUMP_LIGHTING_VECTORS], SURF_LIGHTMAP_VECTORS)
		|| !PBSPV3_LoadTexinfo(pfile, (*pmodel), pheader->lumps[PBSPV3_LUMP_TEXINFO])
		|| !PBSPV3_LoadPlanes(pfile, (*pmodel), pheader->lumps[PBSPV3_LUMP_PLANES])
		|| !PBSPV3_LoadFaces(pfile, (*pmodel), pheader->lumps[PBSPV3_LUMP_FACES])
		|| !PBSPV3_LoadMarksurfaces(pfile, (*pmodel), pheader->lumps[PBSPV3_LUMP_MARKSURFACES])
		|| !PBSPV3_LoadVisibility(pfile, (*pmodel), pheader->lumps[PBSPV3_LUMP_VISIBILITY])
		|| !PBSPV3_LoadBrushData(pfile, (*pmodel), pheader)
		|| !PBSPV3_LoadLeafs(pfile, (*pmodel), pheader)
		|| !PBSPV3_LoadNodes(pfile, (*pmodel), pheader->lumps[PBSPV3_LUMP_NODES])
		|| !PBSPV3_LoadClipnodes(pfile, (*pmodel), pheader->lumps[PBSPV3_LUMP_CLIPNODES])
		|| !PBSPV3_LoadEntities(pfile, (*pmodel), pheader->lumps[PBSPV3_LUMP_ENTITIES])
		|| !PBSPV3_LoadSubmodels(pfile, (*pmodel), pheader->lumps[PBSPV3_LUMP_MODELS])
		|| !PBSPV3_LoadVertexLighting(pfile, (*pmodel), pheader->lumps[PBSPV3_LUMP_VERTEX_LIGHTING_AMBIENT], VERTEX_LIGHTING_AMBIENT)
		|| !PBSPV3_LoadVertexLighting(pfile, (*pmodel), pheader->lumps[PBSPV3_LUMP_VERTEX_LIGHTING_DIFFUSE], VERTEX_LIGHTING_DIFFUSE)
		|| !PBSPV3_LoadVertexLighting(pfile, (*pmodel), pheader->lumps[PBSPV3_LUMP_VERTEX_LIGHTING_VECTORS], VERTEX_LIGHTING_VECTORS)
		|| !PBSPV3_LoadLightGridData(pfile, (*pmodel), pheader->lumps[PBSPV3_LUMP_LIGHTGRID_DATA])
		|| !PBSPV3_LoadDisplacements(pfile, (*pmodel), pheader->lumps[PBSPV3_LUMP_DISPLACEMENTS]))
	{
		delete pmodel;
		return nullptr;
	}

	return pmodel;
}

//=============================================
// @brief
//
//=============================================
bool PBSPV3_LoadVertexes( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump )
{
	// Safeguard against incorrectly compiled BSP
	if(!lump.size)
	{
		Con_EPrintf("%s - Empty lump in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Check if sizes are correct
	if(lump.size % sizeof(dpbspv3vertex_t))
	{
		Con_EPrintf("%s - Inconsistent lump size in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Load the data in
	Uint32 count = lump.size/sizeof(dpbspv3vertex_t);
	const dpbspv3vertex_t* pinverts = reinterpret_cast<const dpbspv3vertex_t*>(pfile + lump.offset);
	mvertex_t* poutverts = new mvertex_t[count];

	model.pvertexes = poutverts;
	model.numvertexes = count;

	for(Uint32 i = 0; i < count; i++)
		Math::VectorCopy(pinverts[i].origin, poutverts[i].origin);

	return true;
}

//=============================================
// @brief
//
//=============================================
bool PBSPV3_LoadEdges( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump )
{
	// Safeguard against incorrectly compiled BSP
	if(!lump.size)
	{
		Con_EPrintf("%s - Empty lump in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Check if sizes are correct
	if(lump.size % sizeof(dpbspv3edge_t))
	{
		Con_EPrintf("%s - Inconsistent lump size in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Load the data in
	Uint32 count = lump.size/sizeof(dpbspv3edge_t);
	const dpbspv3edge_t* pinedges = reinterpret_cast<const dpbspv3edge_t*>(pfile + lump.offset);
	medge_t* poutedges = new medge_t[count];

	model.pedges = poutedges;
	model.numedges = count;

	for(Uint32 i = 0; i < count; i++)
	{
		poutedges[i].vertexes[0] = pinedges[i].vertexes[0];
		poutedges[i].vertexes[1] = pinedges[i].vertexes[1];
	}

	return true;
}

//=============================================
// @brief
//
//=============================================
bool PBSPV3_LoadSurfedges( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump )
{
	// Safeguard against incorrectly compiled BSP
	if(!lump.size)
	{
		Con_EPrintf("%s - Empty lump in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Check if sizes are correct
	if(lump.size % sizeof(Int32))
	{
		Con_EPrintf("%s - Inconsistent lump size in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Load the data in
	Uint32 count = lump.size/sizeof(Int32);
	const Int32* pinedges = reinterpret_cast<const Int32*>(pfile + lump.offset);
	Int32* poutedges = new Int32[count];

	model.psurfedges = poutedges;
	model.numsurfedges = count;

	memcpy(poutedges, pinedges, sizeof(Int32)*count);
	return true;
}

//=============================================
// @brief
//
//=============================================
bool PBSPV3_LoadTextures( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump )
{
	// Safeguard against incorrectly compiled BSP
	if(!lump.size)
	{
		Con_EPrintf("%s - Empty lump in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Get texture counts
	const dmiptexlump_t* pmiptexlump = reinterpret_cast<const dmiptexlump_t*>(pfile + lump.offset);

	model.numtextures = pmiptexlump->nummiptex;
	model.ptextures = new mtexture_t[model.numtextures];

	for(Uint32 i = 0; i < model.numtextures; i++)
	{
		// Get pointer to miptex data
		const dmiptex_t* pmiptex = reinterpret_cast<const dmiptex_t*>(reinterpret_cast<const byte*>(pmiptexlump) + pmiptexlump->dataoffsets[i]);

		// We only get the name and width/height here
		mtexture_t* ptexture = &model.ptextures[i];
		ptexture->name = pmiptex->name;
		ptexture->width = pmiptex->width;
		ptexture->height = pmiptex->height;
	}

	// Handle animated textures
	for(Uint32 i = 0; i < model.numtextures; i++)
	{
		mtexture_t* ptexture = &model.ptextures[i];
		if(ptexture->name[0] != '+' && ptexture->name[0] != '-')
			continue;

		if(ptexture->panim_next)
			continue;

		mtexture_t* panims[MAX_TEXTURE_ANIMS];
		memset(panims, 0, sizeof(mtexture_t*)*MAX_TEXTURE_ANIMS);

		mtexture_t* paltanims[MAX_TEXTURE_ANIMS];
		memset(paltanims, 0, sizeof(mtexture_t*)*MAX_TEXTURE_ANIMS);

		// Check the letter
		Int32 max = ptexture->name[1];
		Int32 altmax = 0;

		if(max >= 'a' && max <= 'z')
			max -= 32;

		if(max < '0' || max > '9')
		{
			if(max < 'A' || max > 'J')
			{
				Con_Printf("Bad animating texture '%s'.\n", ptexture->name.c_str());
				continue;
			}

			altmax = max - 'A';
			max = 0;

			paltanims[altmax] = ptexture;
			altmax++;
		}
		else
		{
			max -= '0';
			altmax = 0;
			panims[max] = ptexture;
			max++;
		}

		// Check ahead
		for(Uint32 j = i+1; j < model.numtextures; j++)
		{
			mtexture_t* pnext = &model.ptextures[j];
			if(!pnext)
				continue;

			if(pnext->name[0] != '+' && pnext->name[0] != '-')
				continue;

			// Only check ones with the same name
			if(qstrcmp(ptexture->name.c_str()+2, pnext->name.c_str()+2))
				continue;

			Int32 num = pnext->name[1];
			if(num >= 'a' && num <= 'z')
				num -= 'a' - 'A';

			if(num < '0' || num > '9')
			{
				if(num < 'A' || num > 'J')
				{
					Con_Printf("Bad animating texture '%s'.\n", ptexture->name.c_str());
					continue;
				}

				num -= 'A';
				paltanims[num] = pnext;
				if((num+1) > altmax)
					altmax = num + 1;
			}
			else
			{
				num -= '0';
				panims[num] = pnext;
				if((num+1) > max)
					max = num+1;
			}
		}

		// Link the animating textures
		for(Int32 j = 0; j < max; j++)
		{
			mtexture_t* pnext = panims[j];
			if(!pnext)
			{
				Con_EPrintf("Missing frame %d animating texture '%s'.\n", j, ptexture->name.c_str());
				continue;
			}

			pnext->anim_min = j;
			pnext->anim_total = max;
			pnext->anim_max = j + 1;
			pnext->panim_next = panims[(j+1)%max];

			if(altmax)
				pnext->palt_anims = paltanims[0];
		}

		// Link alt anims too
		for(Int32 j = 0; j < altmax; j++)
		{
			mtexture_t* pnext = paltanims[j];
			if(!pnext)
			{
				Con_EPrintf("Missing frame %d animating texture '%s'.\n", j, ptexture->name.c_str());
				continue;
			}

			pnext->anim_min = j;
			pnext->anim_total = altmax;
			pnext->anim_max = j + 1;
			pnext->panim_next = paltanims[(j+1)%altmax];

			if(max)
				pnext->palt_anims = panims[0];
		}
	}
	
	return true;
}

//=============================================
// @brief
//
//=============================================
bool PBSPV3_DecompressLightingData( const byte* pfile, const dpbspv3lightingdata_t* plightdata, color24_t*& pdestptr, Uint32& destsize, byte*& poriginaldataptr, Uint32& originalsize, Int32& compression, Int32 compressionlevel )
{
	const byte* prawdatasrc = pfile + plightdata->dataoffset;
	byte* poutputdataptr = new byte[plightdata->noncompressedsize];
	memset(poutputdataptr, 0, sizeof(byte)*plightdata->noncompressedsize);

	// Fill in original info
	poriginaldataptr = new byte[plightdata->datasize];
	memcpy(poriginaldataptr, prawdatasrc, plightdata->datasize);
	originalsize = plightdata->datasize;
	compression = plightdata->compression;
	compressionlevel = plightdata->compressionlevel;

	// Set final data
	switch(plightdata->compression)
	{
	case BSP_LMAP_COMPRESSION_NONE:
		memcpy(poutputdataptr, prawdatasrc, sizeof(byte)*plightdata->noncompressedsize);
		break;
	case BSP_LMAP_COMPRESSION_MINIZ:
		{
			mz_ulong destinationsize = plightdata->noncompressedsize;
			Int32 status = uncompress(poutputdataptr, &destinationsize, prawdatasrc, plightdata->datasize);
			if(status != MZ_OK)
			{
				Con_EPrintf("%s - Miniz uncompress failed with error code %d.\n", __FUNCTION__, status);
				delete[] poutputdataptr;
				return false;
			}

			if(plightdata->noncompressedsize != static_cast<Int32>(destinationsize))
			{
				Con_EPrintf("%s - Miniz uncompress produced inconsistent output size (expected %d, got %d instead).\n", __FUNCTION__, plightdata->noncompressedsize, destinationsize);
				delete[] poutputdataptr;
				return false;
			}
		}
		break;
	}

	pdestptr = reinterpret_cast<color24_t*>(poutputdataptr);
	destsize = plightdata->noncompressedsize;

	return true;
}

//=============================================
// @brief
//
//=============================================
bool PBSPV3_LoadDefaultLighting( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump )
{
	if(!lump.size)
		return true;

	// Check if sizes are correct
	if(lump.size != sizeof(dpbspv3lightingdata_t))
	{
		Con_EPrintf("%s - Inconsistent lump size in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Get raw data and check that the sizes are correct
	const dpbspv3lightingdata_t* plightdata = reinterpret_cast<const dpbspv3lightingdata_t*>(pfile + lump.offset);
	if(plightdata->noncompressedsize % sizeof(color24_t))
	{
		Con_EPrintf("%s - Inconsistent decompressed data size in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	return PBSPV3_DecompressLightingData(pfile, plightdata, model.plightdata[SURF_LIGHTMAP_DEFAULT], model.lightdatasize, 
		model.plightdata_original[SURF_LIGHTMAP_DEFAULT], model.original_lightdatasizes[SURF_LIGHTMAP_DEFAULT],
		model.original_compressiontype[SURF_LIGHTMAP_DEFAULT], model.original_compressionlevel[SURF_LIGHTMAP_DEFAULT]);
}

//=============================================
// @brief
//
//=============================================
bool PBSPV3_LoadLightingDataLayer( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump, surf_lmap_layers_t layer )
{
	if(!lump.size)
		return true;

	// Check if sizes are correct
	if(lump.size != sizeof(dpbspv3lightingdata_t))
	{
		Con_EPrintf("%s - Inconsistent lump size in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Get raw data and check that the sizes are correct
	const dpbspv3lightingdata_t* plightdata = reinterpret_cast<const dpbspv3lightingdata_t*>(pfile + lump.offset);
	if(plightdata->noncompressedsize % sizeof(color24_t))
	{
		Con_EPrintf("%s - Inconsistent decompressed data size in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	Uint32 datasize = 0;
	bool result = PBSPV3_DecompressLightingData(pfile, plightdata, model.plightdata[layer], datasize, 
		model.plightdata_original[layer], model.original_lightdatasizes[layer],
		model.original_compressiontype[layer], model.original_compressionlevel[layer]);

	if(result)
	{
		if(datasize != model.lightdatasize)
		{
			Con_EPrintf("%s - Inconsistent lump size %d in '%s' for light data layer %d, expected size was %d.\n", __FUNCTION__, datasize, model.name.c_str(), layer, model.lightdatasize);
			return false;
		}
	}

	return result;
}

//=============================================
// @brief
//
//=============================================
bool PBSPV3_LoadPlanes( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump )
{
	// Safeguard against incorrectly compiled BSP
	if(!lump.size)
	{
		Con_EPrintf("%s - Empty lump in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Check if sizes are correct
	if(lump.size % sizeof(dpbspv3plane_t))
	{
		Con_EPrintf("%s - Inconsistent lump size in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Load the data in
	Uint32 count = lump.size/sizeof(dpbspv3plane_t);
	const dpbspv3plane_t* pinplanes = reinterpret_cast<const dpbspv3plane_t*>(pfile + lump.offset);
	plane_t* poutplanes = new plane_t[count];

	model.pplanes = poutplanes;
	model.numplanes = count;

	for(Uint32 i = 0; i < count; i++)
	{
		Uint32 bits = 0;
		for(Uint32 j = 0; j < 3; j++)
		{
			poutplanes[i].normal[j] = pinplanes[i].normal[j];
			if(poutplanes[i].normal[j] < 0)
				bits |= 1 << j;
		}

		poutplanes[i].dist = pinplanes[i].dist;
		poutplanes[i].type = static_cast<planetype_t>(pinplanes[i].type);
		poutplanes[i].signbits = bits;
	}

	return true;
}

//=============================================
// @brief
//
//=============================================
bool PBSPV3_LoadTexinfo( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump )
{
	// Safeguard against incorrectly compiled BSP
	if(!lump.size)
	{
		Con_EPrintf("%s - Empty lump in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Check if sizes are correct
	if(lump.size % sizeof(dpbspv3texinfo_t))
	{
		Con_EPrintf("%s - Inconsistent lump size in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Load the data in
	Uint32 count = lump.size/sizeof(dpbspv3texinfo_t);
	const dpbspv3texinfo_t* pintexinfos = reinterpret_cast<const dpbspv3texinfo_t*>(pfile + lump.offset);
	mtexinfo_t* pouttexinfos = new mtexinfo_t[count];
	
	model.ptexinfos = pouttexinfos;
	model.numtexinfos = count;

	for(Uint32 i = 0; i < count; i++)
	{
		// Copy the alignment info
		memcpy(pouttexinfos[i].vecs, pintexinfos[i].vecs, sizeof(Float)*8);

		pouttexinfos[i].flags = pintexinfos[i].flags;
		Int32 textureindex = pintexinfos[i].miptex;

		if(textureindex >= static_cast<Int32>(model.numtextures))
		{
			Con_EPrintf("Invalid texture index '%d' in '%s'.\n", textureindex, model.name.c_str());
			continue;
		}

		pouttexinfos[i].ptexture = &model.ptextures[textureindex];
	}

	return true;
}

//=============================================
// @brief
//
//=============================================
bool PBSPV3_LoadFaces( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump )
{
	// Safeguard against incorrectly compiled BSP
	if(!lump.size)
	{
		Con_EPrintf("%s - Empty lump in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Check if sizes are correct
	if(lump.size % sizeof(dpbspv3face_t))
	{
		Con_EPrintf("%s - Inconsistent lump size in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Load the data in
	Uint32 count = lump.size/sizeof(dpbspv3face_t);
	const dpbspv3face_t* pinfaces = reinterpret_cast<const dpbspv3face_t*>(pfile + lump.offset);
	msurface_t* poutsurfaces = new msurface_t[count];
	
	model.psurfaces = poutsurfaces;
	model.numsurfaces = count;

	// Check if we have any bump map data
	for(Uint32 i = 0; i < count; i++)
	{
		msurface_t* pout = &poutsurfaces[i];

		pout->firstedge = pinfaces[i].firstedge;
		pout->numedges = pinfaces[i].numedges;
		pout->flags = 0;
		pout->base_samplesize = PBSPV3_LM_SAMPLE_SIZE;

		Float sampleScale = pinfaces[i].samplescale;
		if(sampleScale <= 0)
			sampleScale = 1.0;

		pout->lightmapdivider = pout->base_samplesize / sampleScale;

		Uint32 planeindex = pinfaces[i].planenum;
		Int32 side = pinfaces[i].side;
		if(side)
			pout->flags |= SURF_PLANEBACK;

		pout->pplane = &model.pplanes[planeindex];
		
		Int32 texinfoindex = pinfaces[i].texinfo;
		pout->ptexinfo = &model.ptexinfos[texinfoindex];

		if(!BSP_CalcSurfaceExtents(pout, model, PBSPV3_MAX_SURFACE_EXTENTS))
			return false;

		for(Uint32 j = 0; j < MAX_SURFACE_STYLES; j++)
			pout->styles[j] = pinfaces[i].lmstyles[j];
		
		if(pinfaces[i].lightoffset != -1)
		{
			// We only have the base layer
			pout->lightoffset = pinfaces[i].lightoffset;
		}
		else
		{
			// No light data at all
			pout->lightoffset = -1;
		}

		// Flag sky surfaces
		if(!qstrncmp(pout->ptexinfo->ptexture->name.c_str(), "sky", 3))
			pout->flags |= SURF_DRAWSKY;
		else if(pout->ptexinfo->ptexture->name[0] == '!')
			pout->flags |= SURF_DRAWTURB;
	}

	return true;
}

//=============================================
// @brief
//
//=============================================
bool PBSPV3_LoadMarksurfaces( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump )
{
	// Safeguard against incorrectly compiled BSP
	if(!lump.size)
	{
		Con_EPrintf("%s - Empty lump in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Check if sizes are correct
	if(lump.size % sizeof(Int32))
	{
		Con_EPrintf("%s - Inconsistent lump size in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Load the data in
	Uint32 count = lump.size/sizeof(Int32);
	const Uint32* pinmarksurfaces = reinterpret_cast<const Uint32*>(pfile + lump.offset);
	msurface_t** poutmarksurfaces = new msurface_t*[count];

	model.pmarksurfaces = poutmarksurfaces;
	model.nummarksurfaces = count;

	for(Uint32 i = 0; i < count; i++)
	{
		Uint32 surfindex = pinmarksurfaces[i];
		if(surfindex >= count)
		{
			Con_EPrintf("PBSPV3_LoadFaces - Bad surface index.\n");
			return false;
		}

		poutmarksurfaces[i] = &model.psurfaces[surfindex];
	}

	return true;
}

//=============================================
// @brief
//
//=============================================
bool PBSPV3_LoadVisibility( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump )
{
	if(!lump.size)
		return true;

	model.pvisdata = new byte[lump.size];
	memset(model.pvisdata, 0, lump.size);
	model.visdatasize = lump.size;

	memcpy(model.pvisdata, pfile + lump.offset, lump.size);

	return true;
}

//=============================================
// @brief
//
//=============================================
bool PBSPV3_LoadLeafs( const byte* pfile, brushmodel_t& model, const dpbspv3header_t* pheader )
{
	return PBSPV3_LoadLeafs_BrushData(pfile, model, pheader->lumps[PBSPV3_LUMP_LEAFS]);
}

//=============================================
// @brief
//
//=============================================
bool PBSPV3_LoadLeafs_NoBrushData( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump )
{
	// Safeguard against incorrectly compiled BSP
	if(!lump.size)
	{
		Con_EPrintf("%s - Empty lump in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Check if sizes are correct
	if(lump.size % sizeof(dpbspv3leaf_nobrush_t))
	{
		Con_EPrintf("%s - Inconsistent lump size in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Load the data in
	Uint32 count = lump.size/sizeof(dpbspv3leaf_nobrush_t);
	const dpbspv3leaf_nobrush_t* pinleafs = reinterpret_cast<const dpbspv3leaf_nobrush_t*>(pfile + lump.offset);
	mleaf_t* poutleafs = new mleaf_t[count];

	model.pleafs = poutleafs;
	model.numleafs = count;

	for(Uint32 i = 0; i < count; i++)
	{
		mleaf_t* pout = &poutleafs[i];

		for(Uint32 j = 0; j < 3; j++)
		{
			pout->mins[j] = Common::ByteToInt32(reinterpret_cast<const byte*>(&pinleafs[i].mins[j]));
			pout->maxs[j] = Common::ByteToInt32(reinterpret_cast<const byte*>(&pinleafs[i].maxs[j]));
		}

		pout->contents = pinleafs[i].contents;

		Uint32 marksurfindex = pinleafs[i].firstmarksurface;
		pout->pfirstmarksurface = &model.pmarksurfaces[marksurfindex];
		pout->nummarksurfaces = pinleafs[i].nummarksurfaces;

		if(pinleafs[i].visoffset != -1)
			pout->pcompressedvis = model.pvisdata + pinleafs[i].visoffset;
	}

	return true;
}

//=============================================
// @brief
//
//=============================================
bool PBSPV3_LoadLeafs_BrushData( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump )
{
	// Safeguard against incorrectly compiled BSP
	if(!lump.size)
	{
		Con_EPrintf("%s - Empty lump in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Check if sizes are correct
	if(lump.size % sizeof(dpbspv3leaf_brush_t))
	{
		Con_EPrintf("%s - Inconsistent lump size in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Load the data in
	Uint32 count = lump.size/sizeof(dpbspv3leaf_brush_t);
	const dpbspv3leaf_brush_t* pinleafs = reinterpret_cast<const dpbspv3leaf_brush_t*>(pfile + lump.offset);
	mleaf_t* poutleafs = new mleaf_t[count];

	model.pleafs = poutleafs;
	model.numleafs = count;

	for(Uint32 i = 0; i < count; i++)
	{
		mleaf_t* pout = &poutleafs[i];

		for(Uint32 j = 0; j < 3; j++)
		{
			pout->mins[j] = Common::ByteToInt32(reinterpret_cast<const byte*>(&pinleafs[i].mins[j]));
			pout->maxs[j] = Common::ByteToInt32(reinterpret_cast<const byte*>(&pinleafs[i].maxs[j]));
		}

		pout->contents = pinleafs[i].contents;

		Uint32 marksurfindex = pinleafs[i].firstmarksurface;
		pout->pfirstmarksurface = &model.pmarksurfaces[marksurfindex];
		pout->nummarksurfaces = pinleafs[i].nummarksurfaces;

		if(pinleafs[i].visoffset != -1)
			pout->pcompressedvis = model.pvisdata + pinleafs[i].visoffset;

		Uint32 leafbrushindex = pinleafs[i].firstleafbrush;
		pout->pfirstleafbrush = &model.pleafbrushes[leafbrushindex];
		pout->numleafbrushes = pinleafs[i].numleafbrushes;
	}

	return true;
}

//=============================================
// @brief
//
//=============================================
bool PBSPV3_LoadNodes( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump )
{
	// Safeguard against incorrectly compiled BSP
	if(!lump.size)
	{
		Con_EPrintf("%s - Empty lump in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Check if sizes are correct
	if(lump.size % sizeof(dpbspv3node_t))
	{
		Con_EPrintf("%s - Inconsistent lump size in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Load the data in
	Uint32 count = lump.size/sizeof(dpbspv3node_t);
	const dpbspv3node_t* pinnodes = reinterpret_cast<const dpbspv3node_t*>(pfile + lump.offset);
	mnode_t* poutnodes = new mnode_t[count];

	model.pnodes = poutnodes;
	model.numnodes = count;

	for(Uint32 i = 0; i < count; i++)
	{
		mnode_t* pout = &poutnodes[i];

		for(Uint32 j = 0; j < 3; j++)
		{
			pout->mins[j] = Common::ByteToInt32(reinterpret_cast<const byte*>(&pinnodes[i].mins[j]));
			pout->maxs[j] = Common::ByteToInt32(reinterpret_cast<const byte*>(&pinnodes[i].maxs[j]));
		}

		pout->pplane = &model.pplanes[pinnodes[i].planenum];

		pout->firstsurface = pinnodes[i].firstface;
		pout->numsurfaces = pinnodes[i].numfaces;

		for(Uint32 j = 0; j < 2; j++)
		{
			Int32 nodeidx = pinnodes[i].children[j];

			if(nodeidx >= 0)
				pout->pchildren[j] = &model.pnodes[nodeidx];
			else
				pout->pchildren[j] = reinterpret_cast<mnode_t*>(&model.pleafs[-1-nodeidx]);
		}
	}

	return true;
}

//=============================================
// @brief
//
//=============================================
bool PBSPV3_LoadClipnodes( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump )
{
	// Safeguard against incorrectly compiled BSP
	if(!lump.size)
	{
		Con_EPrintf("%s - Empty lump in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Check if sizes are correct
	if(lump.size % sizeof(dpbspv3clipnode_t))
	{
		Con_EPrintf("%s - Inconsistent lump size in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Load the data in
	Uint32 count = lump.size/sizeof(dpbspv3clipnode_t);
	const dpbspv3clipnode_t* pinnodes = reinterpret_cast<const dpbspv3clipnode_t*>(pfile + lump.offset);
	mclipnode_t* poutnodes = new mclipnode_t[count];

	model.pclipnodes = poutnodes;
	model.numclipnodes = count;

	// Set hull 1
	hull_t* phull = &model.hulls[1];
	phull->pclipnodes = poutnodes;
	phull->firstclipnode = 0;
	phull->lastclipnode = count-1;
	phull->pplanes = model.pplanes;
	
	phull->clipmins[0] = -16;
	phull->clipmins[1] = -16;
	phull->clipmins[2] = -36;
	phull->clipmaxs[0] = 16;
	phull->clipmaxs[1] = 16;
	phull->clipmaxs[2] = 36;

	// Set hull 2
	phull = &model.hulls[2];
	phull->pclipnodes = poutnodes;
	phull->firstclipnode = 0;
	phull->lastclipnode = count-1;
	phull->pplanes = model.pplanes;
	
	phull->clipmins[0] = -32;
	phull->clipmins[1] = -32;
	phull->clipmins[2] = -32;
	phull->clipmaxs[0] = 32;
	phull->clipmaxs[1] = 32;
	phull->clipmaxs[2] = 32;

	// Set hull 3
	phull = &model.hulls[3];
	phull->pclipnodes = poutnodes;
	phull->firstclipnode = 0;
	phull->lastclipnode = count-1;
	phull->pplanes = model.pplanes;
	
	phull->clipmins[0] = -16;
	phull->clipmins[1] = -16;
	phull->clipmins[2] = -18;
	phull->clipmaxs[0] = 16;
	phull->clipmaxs[1] = 16;
	phull->clipmaxs[2] = 18;

	for(Uint32 i = 0; i < count; i++)
	{
		poutnodes[i].planenum = pinnodes[i].planenum;
		poutnodes[i].children[0] = pinnodes[i].children[0];
		poutnodes[i].children[1] = pinnodes[i].children[1];
	}

	return true;
}

//=============================================
// @brief
//
//=============================================
bool PBSPV3_LoadEntities( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump )
{
	// Safeguard against incorrectly compiled BSP
	if(!lump.size)
	{
		Con_EPrintf("%s - Empty lump in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	model.pentdata = new Char[lump.size];

	const byte* pdata = pfile + lump.offset;
	memcpy(model.pentdata, pdata, sizeof(Char)*lump.size);
	
	return true;
}

//=============================================
// @brief
//
//=============================================
bool PBSPV3_LoadSubmodels( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump )
{
	// Safeguard against incorrectly compiled BSP
	if(!lump.size)
	{
		Con_EPrintf("%s - Empty lump in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Check if sizes are correct
	if(lump.size % sizeof(dpbspv3model_t))
	{
		Con_EPrintf("%s - Inconsistent lump size in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Load the data in
	Uint32 count = lump.size/sizeof(dpbspv3model_t);
	const dpbspv3model_t* pinmodels = reinterpret_cast<const dpbspv3model_t*>(pfile + lump.offset);
	mmodel_t* poutmodels = new mmodel_t[count];

	model.psubmodels = poutmodels;
	model.numsubmodels = count;

	for(Uint32 i = 0; i < count; i++)
	{
		for(Uint32 j = 0; j < 3; j++)
			poutmodels[i].mins[j] = pinmodels[i].mins[j] - 1;

		for(Uint32 j = 0; j < 3; j++)
			poutmodels[i].maxs[j] = pinmodels[i].maxs[j] - 1;

		for(Uint32 j = 0; j < 3; j++)
			poutmodels[i].origin[j] = pinmodels[i].origin[j];

		for(Uint32 j = 0; j < PBSPV3_MAX_MAP_HULLS; j++)
			poutmodels[i].headnode[j] = pinmodels[i].headnode[j];

		poutmodels[i].visleafs = pinmodels[i].visleafs;
		poutmodels[i].firstface = pinmodels[i].firstface;
		poutmodels[i].numfaces = pinmodels[i].numfaces;
	}

	return true;
}

//=============================================
// @brief
//
//=============================================
bool PBSPV3_LoadVertexLighting(const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump, baked_vertexlight_layers_t layer)
{
	if (!lump.size)
		return true;

	// Check if sizes are correct
	if (lump.size != sizeof(dpbspv3lightingdata_t))
	{
		Con_EPrintf("%s - Inconsistent lump size in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Get raw data and check that the sizes are correct
	const dpbspv3lightingdata_t* plightdata = reinterpret_cast<const dpbspv3lightingdata_t*>(pfile + lump.offset);
	if(plightdata->noncompressedsize % sizeof(color24_t))
	{
		Con_EPrintf("%s - Inconsistent decompressed data size in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	Uint32 datasize = 0;
	bool result = PBSPV3_DecompressLightingData(pfile, plightdata, model.pvertexlightdata[layer], datasize, 
		model.pvertexlightdata_original[layer], model.original_vertexlightdatasizes[layer],
		model.original_vertexlightcompressiontype[layer], model.original_vertexlightcompressionlevel[layer]);

	if(!model.vertexlightdatasize)
	{
		// First lump loaded defines the expected size
		model.vertexlightdatasize = datasize;
	}
	else if(result && datasize != model.vertexlightdatasize)
	{
		Con_EPrintf("%s - Inconsistent lump size %d in '%s' for baked vertex light data layer %d, expected size was %d.\n", __FUNCTION__, datasize, model.name.c_str(), layer, model.lightdatasize);
		return false;
	}

	return result;
}

//=============================================
// @brief
//
//=============================================
bool PBSPV3_LoadLightGridData( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump )
{
	if (!lump.size)
		return true;

	// Get pointer to BSP data
	const dpbspv3lightgridlumpheader_t* psrcgrid = reinterpret_cast<const dpbspv3lightgridlumpheader_t*>(pfile + lump.offset);
	if(psrcgrid->totalsize != lump.size)
	{
		Con_EPrintf("%s - Inconsistent lump size %d in '%s' for light grid data, expected size was %d.\n", __FUNCTION__, lump.size, model.name.c_str(), psrcgrid->totalsize);
		return false;
	}

	// Allocate grid object
	lightgriddata_t* pdestgrid = new lightgriddata_t();

	pdestgrid->rootnodeindex = psrcgrid->rootnodeindex;
	pdestgrid->gridmins = psrcgrid->grid_mins;
	pdestgrid->rawsampledatasize = psrcgrid->rawsampledatasize;

	for(Uint32 i = 0; i < 3; i++)
		pdestgrid->gridscale[i] = 1.0f / static_cast<Float>(psrcgrid->grid_distance[i]);
	
	for(Uint32 i = 0; i < 3; i++)
		pdestgrid->gridsize[i] = psrcgrid->grid_size[i];

	// Copy over raw light sample data
	for(Uint32 i = 0; i < NB_LIGHTGRID_DATA_LAYERS; i++)
	{
		dpbspv3lightingdata_t tmp;
		switch(i)
		{
		case LIGHTGRID_LAYER_VECTORS:
			tmp.compression = psrcgrid->vectorscompressiontype;
			tmp.compressionlevel = psrcgrid->vectorscompressionlevel;
			tmp.noncompressedsize = psrcgrid->rawsampledatasize;
			tmp.datasize = psrcgrid->vectorscompressedsize;
			tmp.dataoffset = psrcgrid->vectorsdataoffset;
			break;
		case LIGHTGRID_LAYER_AMBIENT:
			tmp.compression = psrcgrid->ambientcompressiontype;
			tmp.compressionlevel = psrcgrid->ambientcompressionlevel;
			tmp.noncompressedsize = psrcgrid->rawsampledatasize;
			tmp.datasize = psrcgrid->ambientcompressedsize;
			tmp.dataoffset = psrcgrid->ambientdataoffset;
			break;
		case LIGHTGRID_LAYER_DIFFUSE:
			tmp.compression = psrcgrid->diffusecompressiontype;
			tmp.compressionlevel = psrcgrid->diffusecompressionlevel;
			tmp.noncompressedsize = psrcgrid->rawsampledatasize;
			tmp.datasize = psrcgrid->diffusecompressedsize;
			tmp.dataoffset = psrcgrid->diffusedataoffset;
			break;
		}

		assert(tmp.datasize != 0);

		Uint32 datasize = 0;
		bool result = PBSPV3_DecompressLightingData(reinterpret_cast<const byte*>(psrcgrid), &tmp, pdestgrid->prawsampledata[i], datasize, 
			pdestgrid->psampledata_original[i], pdestgrid->sampledatasize_original[i],
			pdestgrid->original_compressiontypes[i], pdestgrid->original_compressionlevels[i]);

		if(!result)
			return false;
	}

	// Copy nodes
	const dpbspv3lightgridnode_t* psrcnodes = reinterpret_cast<const dpbspv3lightgridnode_t*>(reinterpret_cast<const byte*>(psrcgrid) + psrcgrid->nodesoffset);
	pdestgrid->nodes.resize(psrcgrid->numnodes);

	for(Uint32 i = 0; i < psrcgrid->numnodes; i++)
	{
		const dpbspv3lightgridnode_t* psrcnode = &psrcnodes[i];
		lightgridnode_t& destnode = pdestgrid->nodes[i];

		for(Uint32 j = 0; j < 3; j++)
			destnode.divisionpoint[j] = psrcnode->divisionpoint[j];

		for(Uint32 j = 0; j < 8; j++)
			destnode.children[j] = psrcnode->children[j];
	}

	// Copy leaves
	const dpbspv3lightgridleaf_t* psrcleaves = reinterpret_cast<const dpbspv3lightgridleaf_t*>(reinterpret_cast<const byte*>(psrcgrid) + psrcgrid->leafsoffset);
	pdestgrid->leaves.resize(psrcgrid->numleafs);

	for(Uint32 i = 0; i < psrcgrid->numleafs; i++)
	{
		const dpbspv3lightgridleaf_t* psrcleaf = &psrcleaves[i];
		lightgridleaf_t& destleaf = pdestgrid->leaves[i];

		destleaf.firstsample = psrcleaf->firstsample;
		destleaf.numsamples = psrcleaf->numsamples;
		
		for(Uint32 j = 0; j < 3; j++)
			destleaf.mins[j] = psrcleaf->mins[j];

		for(Uint32 j = 0; j < 3; j++)
			destleaf.size[j] = psrcleaf->size[j];
	}

	// Copy samples
	const dpbspv3lightgridsample_t* psrcsamples = reinterpret_cast<const dpbspv3lightgridsample_t*>(reinterpret_cast<const byte*>(psrcgrid) + psrcgrid->sampleoffset);
	pdestgrid->samples.resize(psrcgrid->numsamples);

	for(Uint32 i = 0; i < psrcgrid->numsamples; i++)
	{
		const dpbspv3lightgridsample_t* psrcsample = &psrcsamples[i];
		lightgridsample_t& destsample = pdestgrid->samples[i];

		for(Uint32 j = 0; j < PBSPV3_MAX_LIGHTMAPS; j++)
			destsample.styles[j] = psrcsample->styles[j];

		// We need this for ALD
		destsample.rawsampleoffset = psrcsample->rawsampleoffset;
		if(destsample.rawsampleoffset != NO_POSITION && (destsample.rawsampleoffset+3) > pdestgrid->rawsampledatasize)
		{
			Con_EPrintf("%s - Raw sample offset %d in sample %d is out of bounds for raw sample data size(%d bytes).\n", __FUNCTION__, destsample.rawsampleoffset, i, pdestgrid->rawsampledatasize);
			delete pdestgrid;
			return false;
		}

		// Set pointers for ease of access
		if(destsample.rawsampleoffset != -1)
		{
			for(Uint32 j = 0; j < NB_LIGHTGRID_DATA_LAYERS; j++)
				destsample.plightdata[j] = reinterpret_cast<byte*>(pdestgrid->prawsampledata[j]) + destsample.rawsampleoffset;
		}
	}
	
	model.plightgrid = pdestgrid;
	return true;
}

//=============================================
// @brief
//
//=============================================
bool PBSPV3_LoadBrushData( const byte* pfile, brushmodel_t& model, const dpbspv3header_t* pheader )
{
	if(!PBSPV3_LoadBrushSides(pfile, model, pheader->lumps[PBSPV3_LUMP_BRUSHSIDES])
		|| !PBSPV3_LoadBrushes(pfile, model, pheader->lumps[PBSPV3_LUMP_BRUSHES])
		|| !PBSPV3_LoadLeafBrushes(pfile, model, pheader->lumps[PBSPV3_LUMP_LEAFBRUSHES]))
		return false;
	else
		return true;
}

//=============================================
// @brief
//
//=============================================
bool PBSPV3_LoadBrushes( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump )
{
	if (!lump.size)
	{
		Con_EPrintf("%s - Empty lump in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Check if sizes are correct
	if (lump.size % sizeof(dpbspv3brush_t))
	{
		Con_EPrintf("%s - Inconsistent lump size in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Load the data in
	Uint32 count = lump.size/sizeof(dpbspv3brush_t);
	const dpbspv3brush_t* pinbrushes = reinterpret_cast<const dpbspv3brush_t*>(pfile + lump.offset);
	mbrush_t* poutbrushes = new mbrush_t[count];

	model.pbrushes = poutbrushes;
	model.numbrushes = count;

	for(Uint32 i = 0; i < count; i++)
	{
		const dpbspv3brush_t* pinbrush = &pinbrushes[i];

		// Sanity check on index
		if(pinbrush->firstside >= model.numbrushsides || (pinbrush->firstside+pinbrush->numsides) > model.numbrushsides)
		{
			Con_EPrintf("%s - Brush %d brush side index '%d' out of range in '%s'.\n", __FUNCTION__, i, pinbrush->firstside, model.name.c_str());
			return false;
		}

		mbrush_t* poutbrush = &model.pbrushes[i];
		poutbrush->contents = pinbrush->contents;
		poutbrush->firstbrushside = pinbrush->firstside;
		poutbrush->numbrushsides = pinbrush->numsides;

		BSP_SetBrushType( model, poutbrush, i );
	}

	return true;
}

//=============================================
// @brief
//
//=============================================
bool PBSPV3_LoadBrushSides( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump )
{
	if (!lump.size)
	{
		Con_EPrintf("%s - Empty lump in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Check if sizes are correct
	if (lump.size % sizeof(dpbspv3brushside_t))
	{
		Con_EPrintf("%s - Inconsistent lump size in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Load the data in
	Uint32 count = lump.size/sizeof(dpbspv3brushside_t);
	const dpbspv3brushside_t* pinbrushsides = reinterpret_cast<const dpbspv3brushside_t*>(pfile + lump.offset);
	mbrushside_t* poutbrushsides = new mbrushside_t[count];

	model.pbrushsides = poutbrushsides;
	model.numbrushsides = count;

	for(Uint32 i = 0; i < count; i++)
	{
		const dpbspv3brushside_t* pinside = &pinbrushsides[i];

		// Sanity check on plane index
		if(pinside->planenum >= model.numplanes)
		{
			Con_EPrintf("%s - Brush side %d plane index '%d' out of range in '%s'.\n", __FUNCTION__, i, pinside->planenum, model.name.c_str());
			return false;
		}

		// Sanity check on texinfo index
		if(pinside->texinfo >= model.numtexinfos)
		{
			Con_EPrintf("%s - Brush side %d texinfo index '%d' out of range in '%s'.\n", __FUNCTION__, i, pinside->texinfo, model.name.c_str());
			return false;
		}

		mbrushside_t* poutside = &model.pbrushsides[i];
		poutside->pplane = &model.pplanes[pinside->planenum];
		poutside->ptexinfo = &model.ptexinfos[pinside->texinfo];

		if(pinside->flags & PBSPV3_BSIDE_FL_PLANEBACK)
			poutside->planeback = true;

		if(pinside->flags & PBSPV3_BSIDE_FL_BEVEL)
			poutside->isbevel = true;
	}

	return true;
}

//=============================================
// @brief
//
//=============================================
bool PBSPV3_LoadLeafBrushes( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump )
{
	if (!lump.size)
	{
		Con_EPrintf("%s - Empty lump in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Check if sizes are correct
	if (lump.size % sizeof(unsigned int))
	{
		Con_EPrintf("%s - Inconsistent lump size in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	// Load the data in
	Uint32 count = lump.size/sizeof(unsigned int);
	const unsigned int* pinleafbrushes = reinterpret_cast<const unsigned int*>(pfile + lump.offset);
	mbrush_t** poutleafbrushes = new mbrush_t*[count];

	model.pleafbrushes = poutleafbrushes;
	model.numleafbrushes = count;

	for(Uint32 i = 0; i < count; i++)
	{
		// Sanity check on index
		Uint32 brushindex = pinleafbrushes[i];
		if(brushindex >= model.numbrushes)
		{
			Con_EPrintf("%s - Leaf brush %d index '%d' out of range in '%s'.\n", __FUNCTION__, i, brushindex, model.name.c_str());
			return false;
		}

		model.pleafbrushes[i] = &model.pbrushes[brushindex];
	}

	return true;
}

//=============================================
// @brief
//
//=============================================
bool PBSPV3_LoadDisplacements(const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump)
{
	if (!lump.size)
		return true;

	// Check if sizes are correct
	if (lump.size < sizeof(dpbspv3dispheader_t))
	{
		Con_EPrintf("%s - Lump size is too small for header in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	const byte* pbuffer = pfile + lump.offset;
	const dpbspv3dispheader_t* pdispheader = reinterpret_cast<const dpbspv3dispheader_t*>(pbuffer);

	Uint32 count_infos = pdispheader->num_disp_infos;
	Uint32 count_verts = pdispheader->num_disp_verts;
	Uint32 count_faces = pdispheader->num_faces;

	Uint32 expected_size = sizeof(dpbspv3dispheader_t) + (count_infos * sizeof(dpbspv3dispinfo_t)) + (count_verts * sizeof(dpbspv3dispvert_t)) + (count_faces * sizeof(Int32));

	// Check if sizes are correct
	if (lump.size != expected_size)
	{
		Con_EPrintf("%s - Inconsistent displacement lump size in '%s'. Expected %u, got %u.\n", __FUNCTION__, model.name.c_str(), expected_size, lump.size);
		return false;
	}

	const dpbspv3dispinfo_t* pinfos = reinterpret_cast<const dpbspv3dispinfo_t*>(pbuffer + sizeof(dpbspv3dispheader_t));
	model.pdispinfo = new mdispinfo_t[count_infos];
	model.numdispinfo = count_infos;
	for (Uint32 i = 0; i < count_infos; i++)
	{
		qstrcpy(model.pdispinfo[i].texture2, pinfos[i].texture2);
	
		model.pdispinfo[i].face_index = pinfos[i].face_index;
		model.pdispinfo[i].power = pinfos[i].power;
		model.pdispinfo[i].vert_start = pinfos[i].vert_start;

		for (Uint32 c = 0; c < 4; c++)
		{
			Math::VectorCopy(pinfos[i].corners[c], model.pdispinfo[i].corners[c]);
		}
	}

	const dpbspv3dispvert_t* pverts = reinterpret_cast<const dpbspv3dispvert_t*>(pbuffer + sizeof(dpbspv3dispheader_t) + (count_infos * sizeof(dpbspv3dispinfo_t)));
	model.pdispverts = new mdispvert_t[count_verts];
	model.numdispverts = count_verts;
	for (Uint32 i = 0; i < count_verts; i++)
	{
		Math::VectorCopy(pverts[i].vector, model.pdispverts[i].vector);
		model.pdispverts[i].distance = pverts[i].distance;
		model.pdispverts[i].alpha = pverts[i].alpha;
	}

	const Int32* pmaps = reinterpret_cast<const Int32*>(pbuffer + sizeof(dpbspv3dispheader_t) + (count_infos * sizeof(dpbspv3dispinfo_t)) + (count_verts * sizeof(dpbspv3dispvert_t)));
	Uint32 limit = (count_faces < model.numsurfaces) ? count_faces : model.numsurfaces;
	for (Uint32 i = 0; i < limit; i++)
		model.psurfaces[i].displacement_id = pmaps[i];

	return true;
}

//=============================================
// @brief
//
//=============================================
bool PBSPV3_LoadChecksum( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump )
{
	if (!lump.size)
		return true;

	// Check if sizes are correct
	if (lump.size != sizeof(dpbspv3checksum_t))
	{
		Con_EPrintf("%s - Inconsistent lump size in '%s'.\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	const dpbspv3checksum_t* pchecksum = reinterpret_cast<const dpbspv3checksum_t*>(pfile + lump.offset);

	Uint64 computed_checksum = 14695981039346656037ULL;
	for (Int32 i = 0; i < lump.offset; i++)
	{
		computed_checksum ^= pfile[i];
		computed_checksum *= 1099511628211ULL;
	}

	if (computed_checksum != pchecksum->checksum)
	{
		Con_EPrintf("%s - BSP file corrupted! ('%s')\n", __FUNCTION__, model.name.c_str());
		return false;
	}

	return true;
}