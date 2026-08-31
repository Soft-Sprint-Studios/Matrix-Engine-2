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
#include "vis.h"
#include "bsp.h"
#include "brush.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>
#include <array>
#include <algorithm>
#include <iostream>
#include <omp.h>

static size_t CompressPVS(const byte* src, size_t srcLength, byte* dest)
{
    byte* destPtr = dest;

    for (size_t j = 0; j < srcLength; j++)
    {
        *destPtr++ = src[j];
        if (src[j])
        {
            continue;
        }

        byte rep = 1;
        for (j++; j < srcLength; j++)
        {
            if (src[j] || rep == 255)
            {
                break;
            }
            rep++;
        }
        *destPtr++ = rep;
        j--;
    }

    return (size_t)(destPtr - dest);
}

struct leaf_sample_t
{
    Float center[3];
    std::vector<std::array<Float, 3>> points;
};

void CalculatePVS(const CRadPipeline* radPipeline)
{
    std::cout << "Computing PVS Visibility...\n";

    size_t totalLeafs = g_BSP.GetLeafCount();
    if (totalLeafs <= 1)
    {
        return;
    }

    size_t numVisLeafs = (g_BSP.GetModelCount() > 0) ? (size_t)g_BSP.GetModel(0).visleafs : (totalLeafs - 1);
    if (numVisLeafs == 0)
    {
        return;
    }

    size_t rowBytes = (numVisLeafs + 7) / 8;
    std::vector<std::vector<byte>> uncompressedPVS(numVisLeafs, std::vector<byte>(rowBytes, 0));
    std::vector<leaf_sample_t> leafSamples(totalLeafs);

    #pragma omp parallel for schedule(static)
    for (int i = 1; i <= (int)numVisLeafs; i++)
    {
        const auto& leaf = g_BSP.GetLeaf(i);
        if (leaf.contents == CONTENTS_SOLID)
        {
            continue;
        }

        auto& ls = leafSamples[i];
        for (Int32 k = 0; k < 3; k++)
        {
            ls.center[k] = (leaf.mins[k] + leaf.maxs[k]) * 0.5f;
        }
        ls.points.push_back({ ls.center[0], ls.center[1], ls.center[2] });

        Float spanX = (Float)(leaf.maxs[0] - leaf.mins[0]);
        Float spanY = (Float)(leaf.maxs[1] - leaf.mins[1]);
        Float spanZ = (Float)(leaf.maxs[2] - leaf.mins[2]);

        Float offsets[12][3] =
        {
            {  0.35f,  0.00f,  0.00f }, { -0.35f,  0.00f,  0.00f },
            {  0.00f,  0.35f,  0.00f }, {  0.00f, -0.35f,  0.00f },
            {  0.00f,  0.00f,  0.35f }, {  0.00f,  0.00f, -0.35f },
            {  0.25f,  0.25f,  0.00f }, { -0.25f, -0.25f,  0.00f },
            {  0.25f, -0.25f,  0.00f }, { -0.25f,  0.25f,  0.00f },
            {  0.00f,  0.25f,  0.25f }, {  0.00f, -0.25f, -0.25f }
        };

        for (int o = 0; o < 12; o++)
        {
            ls.points.push_back({
                ls.center[0] + offsets[o][0] * spanX,
                ls.center[1] + offsets[o][1] * spanY,
                ls.center[2] + offsets[o][2] * spanZ
            });
        }
    }

    #pragma omp parallel for schedule(dynamic, 4)
    for (int i = 0; i < (int)numVisLeafs; i++)
    {
        Int32 srcLeafIdx = i + 1;
        const auto& srcLeaf = g_BSP.GetLeaf(srcLeafIdx);
        if (srcLeaf.contents == CONTENTS_SOLID)
        {
            continue;
        }

        auto& pvsRow = uncompressedPVS[i];
        pvsRow[i >> 3] |= (1 << (i & 7));

        for (size_t j = 0; j < numVisLeafs; j++)
        {
            if (i == (int)j)
            {
                continue;
            }

            Int32 dstLeafIdx = (Int32)j + 1;
            const auto& dstLeaf = g_BSP.GetLeaf(dstLeafIdx);
            if (dstLeaf.contents == CONTENTS_SOLID)
            {
                continue;
            }

            bool adjacent = (srcLeaf.mins[0] <= dstLeaf.maxs[0] + 2 && srcLeaf.maxs[0] >= dstLeaf.mins[0] - 2) &&
                            (srcLeaf.mins[1] <= dstLeaf.maxs[1] + 2 && srcLeaf.maxs[1] >= dstLeaf.mins[1] - 2) &&
                            (srcLeaf.mins[2] <= dstLeaf.maxs[2] + 2 && srcLeaf.maxs[2] >= dstLeaf.mins[2] - 2);

            if (adjacent)
            {
                pvsRow[j >> 3] |= (1 << (j & 7));
                continue;
            }

            bool visible = false;
            if (radPipeline)
            {
                const auto& ptsA = leafSamples[srcLeafIdx].points;
                const auto& ptsB = leafSamples[dstLeafIdx].points;

                for (const auto& ptA : ptsA)
                {
                    for (const auto& ptB : ptsB)
                    {
                        Float hitDist = 0.0f;
                        if (!radPipeline->TraceOcclusion(ptA.data(), ptB.data(), hitDist))
                        {
                            visible = true;
                            break;
                        }
                    }
                    if (visible)
                    {
                        break;
                    }
                }
            }

            if (visible)
            {
                pvsRow[j >> 3] |= (1 << (j & 7));
            }
        }
    }

    std::vector<std::vector<byte>> symmetricPVS(numVisLeafs, std::vector<byte>(rowBytes, 0));

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < (int)numVisLeafs; i++)
    {
        for (size_t j = 0; j < numVisLeafs; j++)
        {
            bool iSeesJ = (uncompressedPVS[i][j >> 3] & (1 << (j & 7))) != 0;
            bool jSeesI = (uncompressedPVS[j][i >> 3] & (1 << (i & 7))) != 0;

            if (iSeesJ || jSeesI)
            {
                symmetricPVS[i][j >> 3] |= (1 << (j & 7));
            }
        }
    }

    std::vector<std::vector<byte>> expandedPVS = symmetricPVS;

    #pragma omp parallel for schedule(dynamic, 8)
    for (int i = 0; i < (int)numVisLeafs; i++)
    {
        for (size_t k = 0; k < numVisLeafs; k++)
        {
            if (!(symmetricPVS[i][k >> 3] & (1 << (k & 7))))
            {
                continue;
            }

            for (size_t b = 0; b < rowBytes; b++)
            {
                expandedPVS[i][b] |= symmetricPVS[k][b];
            }
        }
    }

    std::vector<std::vector<byte>> compressedRows(numVisLeafs);

    #pragma omp parallel for schedule(static)
    for (int i = 0; i < (int)numVisLeafs; i++)
    {
        Int32 leafIdx = i + 1;
        const auto& leaf = g_BSP.GetLeaf(leafIdx);
        if (leaf.contents != CONTENTS_SOLID)
        {
            std::vector<byte> tempBuf(rowBytes * 2 + 16);
            size_t compSize = CompressPVS(expandedPVS[i].data(), rowBytes, tempBuf.data());
            tempBuf.resize(compSize);
            compressedRows[i] = std::move(tempBuf);
        }
    }

    std::vector<byte> compressedVisibilityLump;

    for (size_t i = 1; i < totalLeafs; i++)
    {
        auto& leaf = g_BSP.GetLeaf(i);
        if (i > numVisLeafs || leaf.contents == CONTENTS_SOLID)
        {
            leaf.visoffset = -1;
            continue;
        }

        size_t visIdx = i - 1;
        const auto& rowData = compressedRows[visIdx];

        Int32 currentOffset = (Int32)compressedVisibilityLump.size();
        leaf.visoffset = currentOffset;
        compressedVisibilityLump.insert(compressedVisibilityLump.end(), rowData.begin(), rowData.end());
    }

    g_BSP.SetVisibilityData(compressedVisibilityLump);
}