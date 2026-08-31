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
#include "miniz.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

void CBSPBuilder::AppendLumpData(Int32 lumpIndex, const void* data, size_t size)
{
    if (size == 0 || !data)
    {
        m_header.lumps[lumpIndex].offset = 0;
        m_header.lumps[lumpIndex].size = 0;
        return;
    }

    Int32 offset = (Int32)m_fileBuffer.size();
    m_header.lumps[lumpIndex].offset = offset;
    m_header.lumps[lumpIndex].size = (Int32)size;

    const byte* bytePtr = reinterpret_cast<const byte*>(data);
    m_fileBuffer.insert(m_fileBuffer.end(), bytePtr, bytePtr + size);

    size_t padCount = (4 - (size % 4)) % 4;
    for (size_t i = 0; i < padCount; i++)
    {
        m_fileBuffer.push_back(0);
    }
}

void CBSPBuilder::AppendLightingLump(Int32 lumpIndex, const std::vector<byte>& data)
{
    if (data.empty())
    {
        m_header.lumps[lumpIndex].offset = 0;
        m_header.lumps[lumpIndex].size = 0;
        return;
    }

    mz_ulong uncompressedSize = (mz_ulong)data.size();
    mz_ulong maxCompressedSize = mz_compressBound(uncompressedSize);
    std::vector<byte> compressedData(maxCompressedSize);

    int compLevel = MZ_DEFAULT_LEVEL;
    int status = mz_compress2(compressedData.data(), &maxCompressedSize, data.data(), uncompressedSize, compLevel);

    Int32 baseOffset = (Int32)m_fileBuffer.size();
    Int32 dataOffset = baseOffset + (Int32)sizeof(dpbspv3lightingdata_t);

    dpbspv3lightingdata_t lmapHdr;
    lmapHdr.dataoffset = dataOffset;
    lmapHdr.noncompressedsize = (Int32)uncompressedSize;

    if (status == MZ_OK && maxCompressedSize < uncompressedSize)
    {
        lmapHdr.compression = 1;
        lmapHdr.compressionlevel = compLevel;
        lmapHdr.datasize = (Int32)maxCompressedSize;
    }
    else
    {
        lmapHdr.compression = 0;
        lmapHdr.compressionlevel = 0;
        lmapHdr.datasize = (Int32)uncompressedSize;
    }

    m_header.lumps[lumpIndex].offset = baseOffset;
    m_header.lumps[lumpIndex].size = (Int32)sizeof(dpbspv3lightingdata_t);

    const byte* hdrPtr = reinterpret_cast<const byte*>(&lmapHdr);
    m_fileBuffer.insert(m_fileBuffer.end(), hdrPtr, hdrPtr + sizeof(dpbspv3lightingdata_t));

    if (lmapHdr.compression == 1)
    {
        m_fileBuffer.insert(m_fileBuffer.end(), compressedData.begin(), compressedData.begin() + maxCompressedSize);
    }
    else
    {
        m_fileBuffer.insert(m_fileBuffer.end(), data.begin(), data.end());
    }

    size_t totalSize = sizeof(dpbspv3lightingdata_t) + lmapHdr.datasize;
    size_t padCount = (4 - (totalSize % 4)) % 4;
    for (size_t i = 0; i < padCount; i++)
    {
        m_fileBuffer.push_back(0);
    }
}

Uint64 CBSPBuilder::ComputeChecksum(const byte* buffer, size_t size) const
{
    Uint64 hash = 14695981039346656037ULL;
    for (size_t i = 0; i < size; i++)
    {
        hash ^= buffer[i];
        hash *= 1099511628211ULL;
    }
    return hash;
}

bool CBSPBuilder::ExportFile(const Char* filename)
{
    m_fileBuffer.clear();

    m_fileBuffer.resize(sizeof(dpbspv3header_t), 0);

    AppendLumpData(PBSPV3_LUMP_ENTITIES, m_entityData.c_str(), m_entityData.size() + 1);
    AppendLumpData(PBSPV3_LUMP_PLANES, m_planes.data(), m_planes.size() * sizeof(dpbspv3plane_t));
    AppendLumpData(PBSPV3_LUMP_TEXTURES, m_textures.data(), m_textures.size() * sizeof(dpbspv3texture_t));
    AppendLumpData(PBSPV3_LUMP_VERTEXES, m_vertexes.data(), m_vertexes.size() * sizeof(dpbspv3vertex_t));
    AppendLumpData(PBSPV3_LUMP_VISIBILITY, m_visibility.data(), m_visibility.size());
    AppendLumpData(PBSPV3_LUMP_NODES, m_nodes.data(), m_nodes.size() * sizeof(dpbspv3node_t));
    AppendLumpData(PBSPV3_LUMP_TEXINFO, m_texinfos.data(), m_texinfos.size() * sizeof(dpbspv3texinfo_t));
    AppendLumpData(PBSPV3_LUMP_FACES, m_faces.data(), m_faces.size() * sizeof(dpbspv3face_t));

    AppendLightingLump(PBSPV3_LUMP_LIGHTING_DEFAULT, m_lightmaps[SURF_LIGHTMAP_DEFAULT]);
    AppendLightingLump(PBSPV3_LUMP_LIGHTING_AMBIENT, m_lightmaps[SURF_LIGHTMAP_AMBIENT]);
    AppendLightingLump(PBSPV3_LUMP_LIGHTING_DIFFUSE, m_lightmaps[SURF_LIGHTMAP_DIFFUSE]);
    AppendLightingLump(PBSPV3_LUMP_LIGHTING_VECTORS, m_lightmaps[SURF_LIGHTMAP_VECTORS]);

    AppendLumpData(PBSPV3_LUMP_CLIPNODES, m_clipnodes.data(), m_clipnodes.size() * sizeof(dpbspv3clipnode_t));
    AppendLumpData(PBSPV3_LUMP_LEAFS, m_leafs.data(), m_leafs.size() * sizeof(dpbspv3leaf_brush_t));
    AppendLumpData(PBSPV3_LUMP_MARKSURFACES, m_marksurfaces.data(), m_marksurfaces.size() * sizeof(Uint32));
    AppendLumpData(PBSPV3_LUMP_EDGES, m_edges.data(), m_edges.size() * sizeof(dpbspv3edge_t));
    AppendLumpData(PBSPV3_LUMP_SURFEDGES, m_surfedges.data(), m_surfedges.size() * sizeof(Int32));
    AppendLumpData(PBSPV3_LUMP_MODELS, m_models.data(), m_models.size() * sizeof(dpbspv3model_t));

    AppendLightingLump(PBSPV3_LUMP_VERTEX_LIGHTING_AMBIENT, m_vertexLight[VERTEX_LIGHTING_AMBIENT]);
    AppendLightingLump(PBSPV3_LUMP_VERTEX_LIGHTING_DIFFUSE, m_vertexLight[VERTEX_LIGHTING_DIFFUSE]);
    AppendLightingLump(PBSPV3_LUMP_VERTEX_LIGHTING_VECTORS, m_vertexLight[VERTEX_LIGHTING_VECTORS]);

    AppendLumpData(PBSPV3_LUMP_LIGHTGRID_DATA, m_lightGrid.data(), m_lightGrid.size());
    AppendLumpData(PBSPV3_LUMP_BRUSHES, m_brushes.data(), m_brushes.size() * sizeof(dpbspv3brush_t));
    AppendLumpData(PBSPV3_LUMP_BRUSHSIDES, m_brushsides.data(), m_brushsides.size() * sizeof(dpbspv3brushside_t));
    AppendLumpData(PBSPV3_LUMP_LEAFBRUSHES, m_leafbrushes.data(), m_leafbrushes.size() * sizeof(Uint32));

    if (!m_dispInfos.empty())
    {
        dpbspv3dispheader_t dhdr;
        dhdr.num_disp_infos = (Int32)m_dispInfos.size();
        dhdr.num_disp_verts = (Int32)m_dispVerts.size();
        dhdr.num_faces = (Int32)m_dispFaceMap.size();

        std::vector<byte> dispBuffer;
        size_t hSize = sizeof(dpbspv3dispheader_t);
        size_t iSize = m_dispInfos.size() * sizeof(dpbspv3dispinfo_t);
        size_t vSize = m_dispVerts.size() * sizeof(dpbspv3dispvert_t);
        size_t mSize = m_dispFaceMap.size() * sizeof(Int32);
        dispBuffer.resize(hSize + iSize + vSize + mSize);

        byte* ptr = dispBuffer.data();
        memcpy(ptr, &dhdr, hSize); ptr += hSize;
        memcpy(ptr, m_dispInfos.data(), iSize); ptr += iSize;
        memcpy(ptr, m_dispVerts.data(), vSize); ptr += vSize;
        if (!m_dispFaceMap.empty())
        {
            memcpy(ptr, m_dispFaceMap.data(), mSize);
        }

        AppendLumpData(PBSPV3_LUMP_DISPLACEMENTS, dispBuffer.data(), dispBuffer.size());
    }

    Int32 checksumOffset = (Int32)m_fileBuffer.size();
    m_header.lumps[PBSPV3_LUMP_CHECKSUM].offset = checksumOffset;
    m_header.lumps[PBSPV3_LUMP_CHECKSUM].size = (Int32)sizeof(dpbspv3checksum_t);

    memcpy(m_fileBuffer.data(), &m_header, sizeof(dpbspv3header_t));

    Uint64 fileHash = ComputeChecksum(m_fileBuffer.data(), checksumOffset);
    dpbspv3checksum_t checkLump;
    checkLump.checksum = fileHash;

    const byte* checkPtr = reinterpret_cast<const byte*>(&checkLump);
    m_fileBuffer.insert(m_fileBuffer.end(), checkPtr, checkPtr + sizeof(dpbspv3checksum_t));

    FILE* f = fopen(filename, "wb");
    if (!f)
    {
        std::cerr << "Error: Failed to open '" << filename << "' for writing.\n";
        return false;
    }

    size_t written = fwrite(m_fileBuffer.data(), 1, m_fileBuffer.size(), f);
    fclose(f);

    if (written != m_fileBuffer.size())
    {
        std::cerr << "Error: Incomplete write to '" << filename << "'.\n";
        return false;
    }

    std::cout << "Wrote '" << filename << "' (" << m_fileBuffer.size() << " bytes).\n";
    return true;
}

bool CBSPBuilder::ExportALD(const Char* filename, aldlumptype_t lumpType)
{
    std::vector<byte> existingAldData;
    FILE* pfin = fopen(filename, "rb");
    if (pfin)
    {
        fseek(pfin, 0, SEEK_END);
        long sz = ftell(pfin);
        fseek(pfin, 0, SEEK_SET);
        if (sz > 0)
        {
            existingAldData.resize(sz);
            fread(existingAldData.data(), 1, sz, pfin);
        }
        fclose(pfin);
    }

    const aldheader_t* pOldHeader = nullptr;
    if (existingAldData.size() >= sizeof(aldheader_t))
    {
        pOldHeader = reinterpret_cast<const aldheader_t*>(existingAldData.data());
        if (pOldHeader->header != ALD_HEADER_ENCODED || pOldHeader->version != ALD_HEADER_VERSION)
        {
            pOldHeader = nullptr;
        }
    }

    struct LayerPayload
    {
        std::vector<byte> compressedData;
        size_t rawSize;
    };

    auto CompressRaw = [](const std::vector<byte>& src, LayerPayload& out) {
        out.rawSize = src.size();
        if (src.empty()) 
            return;

        mz_ulong maxDst = mz_compressBound((mz_ulong)src.size());
        out.compressedData.resize(maxDst);
        if (mz_compress2(out.compressedData.data(), &maxDst, src.data(), (mz_ulong)src.size(), MZ_DEFAULT_LEVEL) == MZ_OK)
            out.compressedData.resize(maxDst);
        else
            out.compressedData = src;
        };

    LayerPayload lmapPayloads[NB_SURF_LIGHTMAP_LAYERS];
    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < NB_SURF_LIGHTMAP_LAYERS; i++)
        CompressRaw(m_lightmaps[i], lmapPayloads[i]);

    LayerPayload vertPayloads[NB_BAKED_VERTEXLIGHT_LAYERS];
    #pragma omp parallel for schedule(dynamic)
    for (int i = 0; i < NB_BAKED_VERTEXLIGHT_LAYERS; i++)
        CompressRaw(m_vertexLight[i], vertPayloads[i]);

    LayerPayload gridPayloads[NB_LIGHTGRID_DATA_LAYERS];
    Int32 rawGridSampleSize = 0;
    if (!m_lightGrid.empty() && m_lightGrid.size() >= sizeof(dpbspv3lightgridlumpheader_t))
    {
        const auto* gHdr = reinterpret_cast<const dpbspv3lightgridlumpheader_t*>(m_lightGrid.data());
        rawGridSampleSize = gHdr->rawsampledatasize;

        if (gHdr->ambientcompressedsize > 0)
        {
            gridPayloads[LIGHTGRID_LAYER_AMBIENT].rawSize = gHdr->rawsampledatasize;
            gridPayloads[LIGHTGRID_LAYER_AMBIENT].compressedData.assign(
                m_lightGrid.data() + gHdr->ambientdataoffset,
                m_lightGrid.data() + gHdr->ambientdataoffset + gHdr->ambientcompressedsize);
        }
        if (gHdr->diffusecompressedsize > 0)
        {
            gridPayloads[LIGHTGRID_LAYER_DIFFUSE].rawSize = gHdr->rawsampledatasize;
            gridPayloads[LIGHTGRID_LAYER_DIFFUSE].compressedData.assign(
                m_lightGrid.data() + gHdr->diffusedataoffset,
                m_lightGrid.data() + gHdr->diffusedataoffset + gHdr->diffusecompressedsize);
        }
        if (gHdr->vectorscompressedsize > 0)
        {
            gridPayloads[LIGHTGRID_LAYER_VECTORS].rawSize = gHdr->rawsampledatasize;
            gridPayloads[LIGHTGRID_LAYER_VECTORS].compressedData.assign(
                m_lightGrid.data() + gHdr->vectorsdataoffset,
                m_lightGrid.data() + gHdr->vectorsdataoffset + gHdr->vectorscompressedsize);
        }
    }

    struct LumpEntry
    {
        int type;
        std::vector<byte> layerData[NB_SURF_LIGHTMAP_LAYERS];
        std::vector<byte> vertData[NB_BAKED_VERTEXLIGHT_LAYERS];
        std::vector<byte> gridData[NB_LIGHTGRID_DATA_LAYERS];
    };

    std::vector<LumpEntry> outLumps;

    if (pOldHeader)
    {
        const aldlump_t* pOldLumps = reinterpret_cast<const aldlump_t*>(existingAldData.data() + pOldHeader->lumpoffset);
        for (int i = 0; i < pOldHeader->numlumps; i++)
        {
            if (pOldLumps[i].type == lumpType)
                continue;

            LumpEntry e;
            e.type = pOldLumps[i].type;

            for (int l = 0; l < NB_SURF_LIGHTMAP_LAYERS; l++)
            {
                if (pOldLumps[i].lmaplayeroffsets[l] > 0)
                {
                    const auto* layer = reinterpret_cast<const aldlayer_t*>(existingAldData.data() + pOldLumps[i].lmaplayeroffsets[l]);
                    e.layerData[l].assign(existingAldData.data() + layer->dataoffset, existingAldData.data() + layer->dataoffset + layer->datasize);
                }
            }
            for (int l = 0; l < NB_BAKED_VERTEXLIGHT_LAYERS; l++)
            {
                if (pOldLumps[i].vertexlightlayeroffsets[l] > 0)
                {
                    const auto* layer = reinterpret_cast<const aldlayer_t*>(existingAldData.data() + pOldLumps[i].vertexlightlayeroffsets[l]);
                    e.vertData[l].assign(existingAldData.data() + layer->dataoffset, existingAldData.data() + layer->dataoffset + layer->datasize);
                }
            }
            for (int l = 0; l < NB_LIGHTGRID_DATA_LAYERS; l++)
            {
                if (pOldLumps[i].lightgridlayeroffsets[l] > 0)
                {
                    const auto* layer = reinterpret_cast<const aldlayer_t*>(existingAldData.data() + pOldLumps[i].lightgridlayeroffsets[l]);
                    e.gridData[l].assign(existingAldData.data() + layer->dataoffset, existingAldData.data() + layer->dataoffset + layer->datasize);
                }
            }
            outLumps.push_back(e);
        }
    }

    LumpEntry newLump;
    newLump.type = (int)lumpType;
    for (int l = 0; l < NB_SURF_LIGHTMAP_LAYERS; l++)
        newLump.layerData[l] = lmapPayloads[l].compressedData;
    for (int l = 0; l < NB_BAKED_VERTEXLIGHT_LAYERS; l++)
        newLump.vertData[l] = vertPayloads[l].compressedData;
    for (int l = 0; l < NB_LIGHTGRID_DATA_LAYERS; l++)
        newLump.gridData[l] = gridPayloads[l].compressedData;
    outLumps.push_back(newLump);

    std::vector<byte> outBuffer;
    outBuffer.resize(sizeof(aldheader_t) + outLumps.size() * sizeof(aldlump_t));

    aldheader_t* pHdr = reinterpret_cast<aldheader_t*>(outBuffer.data());
    pHdr->header = ALD_HEADER_ENCODED;
    pHdr->version = ALD_HEADER_VERSION;
    pHdr->flags = 0;
    pHdr->numlumps = (int)outLumps.size();
    pHdr->lumpoffset = (int)sizeof(aldheader_t);
    pHdr->lightdatasize = (int)m_lightmaps[SURF_LIGHTMAP_DEFAULT].size();
    pHdr->vertexlightdatasize = (int)m_vertexLight[VERTEX_LIGHTING_AMBIENT].size();
    pHdr->lightgridsampledatasize = rawGridSampleSize;

    aldlump_t* pLumpTable = reinterpret_cast<aldlump_t*>(outBuffer.data() + pHdr->lumpoffset);

    for (size_t i = 0; i < outLumps.size(); i++)
    {
        pLumpTable[i].type = outLumps[i].type;

        auto WriteLayer = [&](const std::vector<byte>& data, int& offsetField) {
            if (data.empty()) 
            { 
                offsetField = 0; 
                return; 
            }

            offsetField = (int)outBuffer.size();

            aldlayer_t lDesc;
            lDesc.compression = ALD_COMPRESSION_MINIZ;
            lDesc.compressionlevel = MZ_DEFAULT_LEVEL;
            lDesc.datasize = (int)data.size();
            lDesc.dataoffset = offsetField + (int)sizeof(aldlayer_t);

            const byte* descPtr = reinterpret_cast<const byte*>(&lDesc);
            outBuffer.insert(outBuffer.end(), descPtr, descPtr + sizeof(aldlayer_t));
            outBuffer.insert(outBuffer.end(), data.begin(), data.end());
            };

        for (int l = 0; l < NB_SURF_LIGHTMAP_LAYERS; l++)
            WriteLayer(outLumps[i].layerData[l], pLumpTable[i].lmaplayeroffsets[l]);
        for (int l = 0; l < NB_BAKED_VERTEXLIGHT_LAYERS; l++)
            WriteLayer(outLumps[i].vertData[l], pLumpTable[i].vertexlightlayeroffsets[l]);
        for (int l = 0; l < NB_LIGHTGRID_DATA_LAYERS; l++)
            WriteLayer(outLumps[i].gridData[l], pLumpTable[i].lightgridlayeroffsets[l]);
    }

    FILE* pfOut = fopen(filename, "wb");
    if (!pfOut) 
        return false;

    fwrite(outBuffer.data(), 1, outBuffer.size(), pfOut);
    fclose(pfOut);

    std::cout << "Wrote ALD file '" << filename << "' (" << outBuffer.size() << " bytes, " << outLumps.size() << " lumps).\n";
    return true;
}