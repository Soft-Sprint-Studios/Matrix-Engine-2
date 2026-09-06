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
#include "lightmap.h"
#include "bsp.h"
#include <cmath>
#include <cstring>
#include <algorithm>

CLightmapPacker::CLightmapPacker(Int32 atlasSize) :
    m_atlasSize(atlasSize)
{
}

CLightmapPacker::~CLightmapPacker()
{
}

void CLightmapPacker::Reset()
{
    m_pages.clear();
}

bool CLightmapPacker::AllocateBlock(Int32 width, Int32 height, Int32& outPage, Int32& outX, Int32& outY)
{
    if (width > m_atlasSize || height > m_atlasSize)
    {
        return false;
    }

    for (size_t p = 0; p < m_pages.size(); p++)
    {
        page_t& page = m_pages[p];
        Int32 bestY = m_atlasSize;
        Int32 bestIndex = -1;

        for (Int32 i = 0; i <= m_atlasSize - width; i++)
        {
            Int32 maxY = 0;
            for (Int32 j = 0; j < width; j++)
            {
                if (page.skyline[i + j] > maxY)
                {
                    maxY = page.skyline[i + j];
                }
            }

            if (maxY + height <= m_atlasSize && maxY < bestY)
            {
                bestY = maxY;
                bestIndex = i;
            }
        }

        if (bestIndex != -1)
        {
            outPage = (Int32)p;
            outX = bestIndex;
            outY = bestY;

            for (Int32 j = 0; j < width; j++)
            {
                page.skyline[bestIndex + j] = bestY + height;
            }
            return true;
        }
    }

    page_t newPage;
    newPage.skyline.assign(m_atlasSize, 0);

    outPage = (Int32)m_pages.size();
    outX = 0;
    outY = 0;

    for (Int32 j = 0; j < width; j++)
    {
        newPage.skyline[j] = height;
    }

    m_pages.push_back(newPage);
    return true;
}

void CalculateFaceLightmapExtents(const poly_face_t& polyFace, Int32 bspFaceIndex, lightmap_face_t& outLm)
{
    outLm.bspFaceIndex = bspFaceIndex;
    outLm.planeIndex = polyFace.planeIndex;
    outLm.texinfoIndex = polyFace.texinfoIndex;
    outLm.lightOffset = -1;
    outLm.atlasPage = 0;
    outLm.atlasX = 0;
    outLm.atlasY = 0;

    Float sampleScale = g_BSP.GetFace(bspFaceIndex).samplescale;
    if (sampleScale <= 0.0f)
    {
        sampleScale = 1.0f;
    }

    outLm.lightmapDivider = roundf((Float)MBSPV1_LM_SAMPLE_SIZE / sampleScale);
    if (outLm.lightmapDivider < 1.0f)
    {
        outLm.lightmapDivider = 1.0f;
    }

    const dmbspv1texinfo_t& tx = g_BSP.GetTexinfo(polyFace.texinfoIndex);

    outLm.exactMins[0] = outLm.exactMins[1] = 9999999.0f;
    outLm.exactMaxs[0] = outLm.exactMaxs[1] = -9999999.0f;

    for (const auto& v : polyFace.verts)
    {
        Float s = v.pos[0] * tx.vecs[0][0] + v.pos[1] * tx.vecs[0][1] + v.pos[2] * tx.vecs[0][2] + tx.vecs[0][3];
        Float t = v.pos[0] * tx.vecs[1][0] + v.pos[1] * tx.vecs[1][1] + v.pos[2] * tx.vecs[1][2] + tx.vecs[1][3];

        if (s < outLm.exactMins[0]) 
            outLm.exactMins[0] = s;
        if (s > outLm.exactMaxs[0]) 
            outLm.exactMaxs[0] = s;
        if (t < outLm.exactMins[1]) 
            outLm.exactMins[1] = t;
        if (t > outLm.exactMaxs[1]) 
            outLm.exactMaxs[1] = t;
    }

    Float spanS = outLm.exactMaxs[0] - outLm.exactMins[0];
    Float spanT = outLm.exactMaxs[1] - outLm.exactMins[1];

    Int32 minS = (Int32)floorf(outLm.exactMins[0] / outLm.lightmapDivider + 0.001f);
    Int32 maxS = (Int32)ceilf(outLm.exactMaxs[0] / outLm.lightmapDivider - 0.001f);
    Int32 minT = (Int32)floorf(outLm.exactMins[1] / outLm.lightmapDivider + 0.001f);
    Int32 maxT = (Int32)ceilf(outLm.exactMaxs[1] / outLm.lightmapDivider - 0.001f);

    outLm.textureMins[0] = (Int32)(minS * outLm.lightmapDivider);
    outLm.textureMins[1] = (Int32)(minT * outLm.lightmapDivider);

    outLm.extents[0] = (Int32)((maxS - minS) * outLm.lightmapDivider);
    outLm.extents[1] = (Int32)((maxT - minT) * outLm.lightmapDivider);

    outLm.luxelWidth = std::max(1, (maxS - minS) + 1);
    outLm.luxelHeight = std::max(1, (maxT - minT) + 1);
    outLm.totalLuxels = outLm.luxelWidth * outLm.luxelHeight;

    GenerateLuxelWorldCoordinates(outLm, polyFace);
}

static void GenerateDisplacementLuxels(lightmap_face_t& lmFace, Int32 dispIdx, const Float texorg[3], const Float textoworld[2][3], const Float normal[3], Int32 minS, Int32 minT)
{
    const auto& di = g_BSP.GetDispInfo(dispIdx);
    Int32 N = 1 << di.power;
    Int32 K = N + 1;
    std::vector<Float> grid(K * K * 3);

    for (Int32 gy = 0; gy < K; gy++)
    {
        Float gv = (Float)gy / (Float)N;
        for (Int32 gx = 0; gx < K; gx++)
        {
            Float gu = (Float)gx / (Float)N;
            Float base[3];
            for (Int32 c = 0; c < 3; c++)
            {
                base[c] = (1.0f - gu) * (1.0f - gv) * di.corners[0][c] +
                           gu * (1.0f - gv) * di.corners[1][c] +
                           gu * gv * di.corners[2][c] +
                           (1.0f - gu) * gv * di.corners[3][c];
            }
            Int32 vIdx = di.vert_start + gy * K + gx;
            const auto& dv = g_BSP.GetDispVert(vIdx);
            Int32 gOff = (gy * K + gx) * 3;
            grid[gOff + 0] = base[0] + dv.vector[0] * dv.distance;
            grid[gOff + 1] = base[1] + dv.vector[1] * dv.distance;
            grid[gOff + 2] = base[2] + dv.vector[2] * dv.distance;
        }
    }

    Float rayDir[3] = { -normal[0], -normal[1], -normal[2] };

    for (Int32 y = 0; y < lmFace.luxelHeight; y++)
    {
        Float sampleT = (minT + y + 0.5f) * lmFace.lightmapDivider;
        Float rowPos[3] = {
            texorg[0] + sampleT * textoworld[1][0],
            texorg[1] + sampleT * textoworld[1][1],
            texorg[2] + sampleT * textoworld[1][2]
        };

        for (Int32 x = 0; x < lmFace.luxelWidth; x++)
        {
            Int32 idx = y * lmFace.luxelWidth + x;
            Float sampleS = (minS + x + 0.5f) * lmFace.lightmapDivider;

            Float flatPos[3] = {
                rowPos[0] + sampleS * textoworld[0][0],
                rowPos[1] + sampleS * textoworld[0][0],
                rowPos[2] + sampleS * textoworld[0][2]
            };

            Float rayStart[3] = { flatPos[0] + normal[0] * 2048.0f, flatPos[1] + normal[1] * 2048.0f, flatPos[2] + normal[2] * 2048.0f };
            Float bestT = 999999.0f;
            Float hitPos[3] = { flatPos[0], flatPos[1], flatPos[2] };
            Float hitNormal[3] = { normal[0], normal[1], normal[2] };

            for (Int32 gy = 0; gy < N; gy++)
            {
                for (Int32 gx = 0; gx < N; gx++)
                {
                    Int32 i00 = (gy * K + gx) * 3, i10 = (gy * K + (gx + 1)) * 3;
                    Int32 i01 = ((gy + 1) * K + gx) * 3, i11 = ((gy + 1) * K + (gx + 1)) * 3;
                    const Float* tris[2][3] = { { &grid[i00], &grid[i01], &grid[i11] }, { &grid[i00], &grid[i11], &grid[i10] } };

                    for (int t = 0; t < 2; t++)
                    {
                        Float e1[3] = { tris[t][1][0] - tris[t][0][0], tris[t][1][1] - tris[t][0][1], tris[t][1][2] - tris[t][0][2] };
                        Float e2[3] = { tris[t][2][0] - tris[t][0][0], tris[t][2][1] - tris[t][0][1], tris[t][2][2] - tris[t][0][2] };
                        Float h[3] = { rayDir[1] * e2[2] - rayDir[2] * e2[1], rayDir[2] * e2[0] - rayDir[0] * e2[2], rayDir[0] * e2[1] - rayDir[1] * e2[0] };
                        Float a = e1[0] * h[0] + e1[1] * h[1] + e1[2] * h[2];
                        if (fabsf(a) < 1e-6f) 
                            continue;

                        Float f = 1.0f / a;
                        Float s[3] = { rayStart[0] - tris[t][0][0], rayStart[1] - tris[t][0][1], rayStart[2] - tris[t][0][2] };
                        Float u = f * (s[0] * h[0] + s[1] * h[1] + s[2] * h[2]);
                        if (u < 0.0f || u > 1.0f) 
                            continue;

                        Float q[3] = { s[1] * e1[2] - s[2] * e1[1], s[2] * e1[0] - s[0] * e1[2], s[0] * e1[1] - s[1] * e1[0] };
                        Float v = f * (rayDir[0] * q[0] + rayDir[1] * q[1] + rayDir[2] * q[2]);
                        if (v < 0.0f || u + v > 1.0f)
                            continue;

                        Float hitT = f * (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]);
                        if (hitT > 0.001f && hitT < bestT)
                        {
                            bestT = hitT;
                            hitPos[0] = rayStart[0] + rayDir[0] * hitT;
                            hitPos[1] = rayStart[1] + rayDir[1] * hitT;
                            hitPos[2] = rayStart[2] + rayDir[2] * hitT;

                            Float tn[3] = { e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0] };
                            Float tlen = sqrtf(tn[0] * tn[0] + tn[1] * tn[1] + tn[2] * tn[2]);
                            if (tlen > 1e-5f)
                            {
                                hitNormal[0] = tn[0] / tlen;
                                hitNormal[1] = tn[1] / tlen;
                                hitNormal[2] = tn[2] / tlen;
                            }
                        }
                    }
                }
            }

            luxel_coord_t& coord = lmFace.sampleCoords[idx];
            coord.normal[0] = hitNormal[0]; coord.normal[1] = hitNormal[1]; coord.normal[2] = hitNormal[2];
            coord.worldPos[0] = hitPos[0] + hitNormal[0] * 0.5f;
            coord.worldPos[1] = hitPos[1] + hitNormal[1] * 0.5f;
            coord.worldPos[2] = hitPos[2] + hitNormal[2] * 0.5f;
        }
    }
}

void GenerateLuxelWorldCoordinates(lightmap_face_t& lmFace, const poly_face_t& polyFace)
{
    lmFace.sampleCoords.resize(lmFace.totalLuxels);

    const dmbspv1texinfo_t& tx = g_BSP.GetTexinfo(polyFace.texinfoIndex);
    const dmbspv1face_t& bspFace = g_BSP.GetFace(lmFace.bspFaceIndex);
    const dmbspv1plane_t& pl = g_BSP.GetPlane(polyFace.planeIndex);

    Float sideSign = (bspFace.side != 0) ? -1.0f : 1.0f;
    Float normal[3] = { pl.normal[0] * sideSign, pl.normal[1] * sideSign, pl.normal[2] * sideSign };

    Float texnormal[3];
    texnormal[0] = tx.vecs[1][1] * tx.vecs[0][2] - tx.vecs[1][2] * tx.vecs[0][1];
    texnormal[1] = tx.vecs[1][2] * tx.vecs[0][0] - tx.vecs[1][0] * tx.vecs[0][2];
    texnormal[2] = tx.vecs[1][0] * tx.vecs[0][1] - tx.vecs[1][1] * tx.vecs[0][0];

    Float tlen = sqrtf(texnormal[0] * texnormal[0] + texnormal[1] * texnormal[1] + texnormal[2] * texnormal[2]);
    if (tlen > 0.00001f)
    {
        texnormal[0] /= tlen;
        texnormal[1] /= tlen;
        texnormal[2] /= tlen;
    }

    Float distscale = texnormal[0] * normal[0] + texnormal[1] * normal[1] + texnormal[2] * normal[2];
    if (distscale < 0.0f)
    {
        distscale = -distscale;
        texnormal[0] = -texnormal[0];
        texnormal[1] = -texnormal[1];
        texnormal[2] = -texnormal[2];
    }
    distscale = (distscale > 0.00001f) ? (1.0f / distscale) : 1.0f;

    Float textoworld[2][3];
    for (Int32 i = 0; i < 2; i++)
    {
        const Float* otherAxis = tx.vecs[!i];
        textoworld[i][0] = otherAxis[1] * normal[2] - otherAxis[2] * normal[1];
        textoworld[i][1] = otherAxis[2] * normal[0] - otherAxis[0] * normal[2];
        textoworld[i][2] = otherAxis[0] * normal[1] - otherAxis[1] * normal[0];

        Float len = textoworld[i][0] * tx.vecs[i][0] + textoworld[i][1] * tx.vecs[i][1] + textoworld[i][2] * tx.vecs[i][2];
        if (fabsf(len) > 0.00001f)
        {
            textoworld[i][0] /= len;
            textoworld[i][1] /= len;
            textoworld[i][2] /= len;
        }
    }

    Float texorg[3];
    for (Int32 i = 0; i < 3; i++)
    {
        texorg[i] = -tx.vecs[0][3] * textoworld[0][i] - tx.vecs[1][3] * textoworld[1][i];
    }

    Float dist = (texorg[0] * normal[0] + texorg[1] * normal[1] + texorg[2] * normal[2]) - (pl.dist * sideSign);
    dist *= distscale;
    texorg[0] -= dist * texnormal[0];
    texorg[1] -= dist * texnormal[1];
    texorg[2] -= dist * texnormal[2];
    Int32 minS = (Int32)floorf(lmFace.exactMins[0] / lmFace.lightmapDivider + 0.001f);
    Int32 minT = (Int32)floorf(lmFace.exactMins[1] / lmFace.lightmapDivider + 0.001f);

    Int32 dispIdx = g_BSP.GetFaceDispIndex(lmFace.bspFaceIndex);
    if (dispIdx >= 0)
    {
        GenerateDisplacementLuxels(lmFace, dispIdx, texorg, textoworld, normal, minS, minT);
        return;
    }

    Float normOffset[3] = { normal[0] * 0.5f, normal[1] * 0.5f, normal[2] * 0.5f };

    for (Int32 y = 0; y < lmFace.luxelHeight; y++)
    {
        Float sampleT = (minT + y + 0.5f) * lmFace.lightmapDivider;
        Float rowPos[3] = {
            texorg[0] + sampleT * textoworld[1][0] + normOffset[0],
            texorg[1] + sampleT * textoworld[1][1] + normOffset[1],
            texorg[2] + sampleT * textoworld[1][2] + normOffset[2]
        };

        for (Int32 x = 0; x < lmFace.luxelWidth; x++)
        {
            Int32 idx = y * lmFace.luxelWidth + x;
            Float sampleS = (minS + x + 0.5f) * lmFace.lightmapDivider;

            luxel_coord_t& coord = lmFace.sampleCoords[idx];
            coord.worldPos[0] = rowPos[0] + sampleS * textoworld[0][0];
            coord.worldPos[1] = rowPos[1] + sampleS * textoworld[0][1];
            coord.worldPos[2] = rowPos[2] + sampleS * textoworld[0][2];
            coord.normal[0] = normal[0];
            coord.normal[1] = normal[1];
            coord.normal[2] = normal[2];
        }
    }
}

void AllocateAllFaceLightmaps(std::vector<lightmap_face_t>& inOutFaceLightmaps)
{
    CLightmapPacker packer(2048);

    size_t currentLightOffset = 0;

    for (auto& lm : inOutFaceLightmaps)
    {
        if (g_BSP.GetTexinfo(lm.texinfoIndex).flags & 1)
        {
            g_BSP.SetFaceLightOffset(lm.bspFaceIndex, -1);
            continue;
        }

        packer.AllocateBlock(lm.luxelWidth, lm.luxelHeight, lm.atlasPage, lm.atlasX, lm.atlasY);

        lm.lightOffset = (Int32)currentLightOffset;
        g_BSP.SetFaceLightOffset(lm.bspFaceIndex, lm.lightOffset);

        Int32 numStyles = 1;
        for (Int32 i = 1; i < MBSPV1_MAX_LIGHTMAPS; i++)
        {
            if (g_BSP.GetFace(lm.bspFaceIndex).lmstyles[i] != 255)
            {
                numStyles++;
            }
        }

        size_t faceByteSize = (size_t)lm.totalLuxels * sizeof(Float) * 3 * numStyles;
        currentLightOffset += faceByteSize;
    }

    std::vector<byte> defLight(currentLightOffset, 0);
    std::vector<byte> ambLight(currentLightOffset, 0);
    std::vector<byte> diffLight(currentLightOffset, 0);
    std::vector<byte> vecLight(currentLightOffset, 128);
    for (size_t i = 2; i < currentLightOffset; i += 3)
    {
        vecLight[i] = 255;
    }

    g_BSP.SetLightmapLayer(SURF_LIGHTMAP_DEFAULT, defLight);
    g_BSP.SetLightmapLayer(SURF_LIGHTMAP_AMBIENT, ambLight);
    g_BSP.SetLightmapLayer(SURF_LIGHTMAP_DIFFUSE, diffLight);
    g_BSP.SetLightmapLayer(SURF_LIGHTMAP_VECTORS, vecLight);
}