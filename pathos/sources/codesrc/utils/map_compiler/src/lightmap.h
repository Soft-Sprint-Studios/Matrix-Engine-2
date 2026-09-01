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
#ifndef LIGHTMAP_H
#define LIGHTMAP_H

#include "datatypes.h"
#include "mbspv1file.h"
#include "brush.h"
#include <vector>

struct luxel_coord_t
{
    Float worldPos[3];
    Float normal[3];
};

struct lightmap_face_t
{
    Int32 bspFaceIndex;
    Int32 planeIndex;
    Int32 texinfoIndex;
    Float lightmapDivider;

    Float exactMins[2];
    Float exactMaxs[2];

    Int32 textureMins[2];
    Int32 extents[2];

    Int32 luxelWidth;
    Int32 luxelHeight;
    Int32 totalLuxels;

    Int32 lightOffset;
    Int32 atlasPage;
    Int32 atlasX;
    Int32 atlasY;

    std::vector<luxel_coord_t> sampleCoords;
};

class CLightmapPacker
{
public:
    CLightmapPacker(Int32 atlasSize = 2048);
    ~CLightmapPacker();

    void Reset();
    bool AllocateBlock(Int32 width, Int32 height, Int32& outPage, Int32& outX, Int32& outY);

private:
    struct page_t
    {
        std::vector<Int32> skyline;
    };

    Int32 m_atlasSize;
    std::vector<page_t> m_pages;
};

void CalculateFaceLightmapExtents(const poly_face_t& polyFace, Int32 bspFaceIndex, lightmap_face_t& outLm);
void GenerateLuxelWorldCoordinates(lightmap_face_t& lmFace, const poly_face_t& polyFace);
void AllocateAllFaceLightmaps(std::vector<lightmap_face_t>& inOutFaceLightmaps);

#endif // LIGHTMAP_H