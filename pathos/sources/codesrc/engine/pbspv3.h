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
#ifndef PBSPV3_H
#define PBSPV3_H

//
// Load functions
//
brushmodel_t* PBSPV3_Load( const byte* pfile, const dpbspv3header_t* pheader, const Char* pstrFilename );

//
// Functions for loading specific lumps
//
extern bool PBSPV3_LoadVertexes( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump );
extern bool PBSPV3_LoadEdges( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump );
extern bool PBSPV3_LoadSurfedges( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump );
extern bool PBSPV3_LoadTextures( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump );
extern bool PBSPV3_LoadDefaultLighting( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump );
extern bool PBSPV3_LoadLightingDataLayer( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump, surf_lmap_layers_t layer );
extern bool PBSPV3_LoadPlanes( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump );
extern bool PBSPV3_LoadTexinfo( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump );
extern bool PBSPV3_LoadFaces( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump );
extern bool PBSPV3_LoadMarksurfaces( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump );
extern bool PBSPV3_LoadVisibility( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump );
extern bool PBSPV3_LoadBrushData( const byte* pfile, brushmodel_t& model, const dpbspv3header_t* pheader );
extern bool PBSPV3_LoadLeafs( const byte* pfile, brushmodel_t& model, const dpbspv3header_t* pheader );
extern bool PBSPV3_LoadLeafs_NoBrushData( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump );
extern bool PBSPV3_LoadLeafs_BrushData( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump );
extern bool PBSPV3_LoadNodes( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump );
extern bool PBSPV3_LoadClipnodes( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump );
extern bool PBSPV3_LoadEntities( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump );
extern bool PBSPV3_LoadSubmodels( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump );
extern bool PBSPV3_LoadVertexLighting( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump, baked_vertexlight_layers_t layer );
extern bool PBSPV3_DecompressLightingData( const byte* pfile, const dpbspv3lightingdata_t* plightdata, color24_t*& pdestptr, Uint32& destsize, byte*& poriginaldataptr, Uint32& originalsize, Int32& compression, Int32 compressionlevel );
extern bool PBSPV3_LoadLightGridData( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump );
extern bool PBSPV3_LoadBrushes( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump );
extern bool PBSPV3_LoadBrushSides( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump );
extern bool PBSPV3_LoadLeafBrushes( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump );
extern bool PBSPV3_LoadDisplacements( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump );
extern bool PBSPV3_LoadChecksum( const byte* pfile, brushmodel_t& model, const dpbspv3lump_t& lump );
#endif //PBSPV3_H