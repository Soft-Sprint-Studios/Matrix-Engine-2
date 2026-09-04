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
#ifndef MBSPV1_H
#define MBSPV1_H

//
// Load functions
//
brushmodel_t* MBSPV1_Load( const byte* pfile, const dmbspv1header_t* pheader, const Char* pstrFilename );

//
// Functions for loading specific lumps
//
extern bool MBSPV1_LoadVertexes( const byte* pfile, brushmodel_t& model, const dmbspv1lump_t& lump );
extern bool MBSPV1_LoadEdges( const byte* pfile, brushmodel_t& model, const dmbspv1lump_t& lump );
extern bool MBSPV1_LoadSurfedges( const byte* pfile, brushmodel_t& model, const dmbspv1lump_t& lump );
extern bool MBSPV1_LoadTextures( const byte* pfile, brushmodel_t& model, const dmbspv1lump_t& lump );
extern bool MBSPV1_LoadDefaultLighting( const byte* pfile, brushmodel_t& model, const dmbspv1lump_t& lump );
extern bool MBSPV1_LoadLightingDataLayer( const byte* pfile, brushmodel_t& model, const dmbspv1lump_t& lump, surf_lmap_layers_t layer );
extern bool MBSPV1_LoadPlanes( const byte* pfile, brushmodel_t& model, const dmbspv1lump_t& lump );
extern bool MBSPV1_LoadTexinfo( const byte* pfile, brushmodel_t& model, const dmbspv1lump_t& lump );
extern bool MBSPV1_LoadFaces( const byte* pfile, brushmodel_t& model, const dmbspv1lump_t& lump );
extern bool MBSPV1_LoadMarksurfaces( const byte* pfile, brushmodel_t& model, const dmbspv1lump_t& lump );
extern bool MBSPV1_LoadVisibility( const byte* pfile, brushmodel_t& model, const dmbspv1lump_t& lump );
extern bool MBSPV1_LoadLeafs( const byte* pfile, brushmodel_t& model, const dmbspv1lump_t& lump );
extern bool MBSPV1_LoadNodes( const byte* pfile, brushmodel_t& model, const dmbspv1lump_t& lump );
extern bool MBSPV1_LoadClipnodes( const byte* pfile, brushmodel_t& model, const dmbspv1lump_t& lump );
extern bool MBSPV1_LoadEntities( const byte* pfile, brushmodel_t& model, const dmbspv1lump_t& lump );
extern bool MBSPV1_LoadSubmodels( const byte* pfile, brushmodel_t& model, const dmbspv1lump_t& lump );
extern bool MBSPV1_LoadVertexLighting( const byte* pfile, brushmodel_t& model, const dmbspv1lump_t& lump, baked_vertexlight_layers_t layer );
extern bool MBSPV1_DecompressLightingData( const byte* pfile, const dmbspv1lightingdata_t* plightdata, color24_t*& pdestptr, Uint32& destsize, byte*& poriginaldataptr, Uint32& originalsize, Int32& compression, Int32 compressionlevel );
extern bool MBSPV1_LoadLightGridData( const byte* pfile, brushmodel_t& model, const dmbspv1lump_t& lump );
extern bool MBSPV1_LoadBrushes( const byte* pfile, brushmodel_t& model, const dmbspv1lump_t& lump );
extern bool MBSPV1_LoadBrushSides( const byte* pfile, brushmodel_t& model, const dmbspv1lump_t& lump );
extern bool MBSPV1_LoadLeafBrushes( const byte* pfile, brushmodel_t& model, const dmbspv1lump_t& lump );
extern bool MBSPV1_LoadDisplacements( const byte* pfile, brushmodel_t& model, const dmbspv1lump_t& lump );
extern bool MBSPV1_LoadChecksum( const byte* pfile, brushmodel_t& model, const dmbspv1lump_t& lump );
#endif //MBSPV1_H