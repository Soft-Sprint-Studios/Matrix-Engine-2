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
#include "rad_phong.h"
#include "bsp.h"
#include <cmath>
#include <array>
#include <vector>
#include <algorithm>
#include <omp.h>

struct phong_poly_t
{
    Int32 bspFaceIndex;
    Float normal[3];
    std::vector<std::array<Float, 3>> verts;
    std::vector<std::array<Float, 3>> vertNormals;
};

static void InterpolateTriangleBarycentric(const std::array<Float, 3>& A, const std::array<Float, 3>& B, const std::array<Float, 3>& C,
                                          const std::array<Float, 3>& NA, const std::array<Float, 3>& NB, const std::array<Float, 3>& NC,
                                          const Float P[3], Float outNormal[3], Float& outDistSq)
{
    Float v0[3] = { B[0] - A[0], B[1] - A[1], B[2] - A[2] };
    Float v1[3] = { C[0] - A[0], C[1] - A[1], C[2] - A[2] };
    Float v2[3] = { P[0] - A[0], P[1] - A[1], P[2] - A[2] };

    Float d00 = v0[0] * v0[0] + v0[1] * v0[1] + v0[2] * v0[2];
    Float d01 = v0[0] * v1[0] + v0[1] * v1[1] + v0[2] * v1[2];
    Float d11 = v1[0] * v1[0] + v1[1] * v1[1] + v1[2] * v1[2];
    Float d20 = v2[0] * v0[0] + v2[1] * v0[1] + v2[2] * v0[2];
    Float d21 = v2[0] * v1[0] + v2[1] * v1[1] + v2[2] * v1[2];

    Float denom = d00 * d11 - d01 * d01;
    if (fabsf(denom) < 1e-6f)
    {
        outDistSq = 1e9f;
        return;
    }

    Float v = (d11 * d20 - d01 * d21) / denom;
    Float w = (d00 * d21 - d01 * d20) / denom;
    Float u = 1.0f - v - w;

    Float uC = std::clamp(u, 0.0f, 1.0f);
    Float vC = std::clamp(v, 0.0f, 1.0f);
    Float wC = std::clamp(w, 0.0f, 1.0f);
    Float sum = uC + vC + wC;
    if (sum > 0.0001f)
    {
        uC /= sum; vC /= sum; wC /= sum;
    }

    Float du = u - uC;
    Float dv = v - vC;
    Float dw = w - wC;
    outDistSq = du * du + dv * dv + dw * dw;

    outNormal[0] = uC * NA[0] + vC * NB[0] + wC * NC[0];
    outNormal[1] = uC * NA[1] + vC * NB[1] + wC * NC[1];
    outNormal[2] = uC * NA[2] + vC * NB[2] + wC * NC[2];
}

void SmoothFaceNormals(std::vector<lightmap_face_t>& faceLightmaps, Float maxAngleDegrees)
{
    const Float minDot = cosf(maxAngleDegrees * (M_PI / 180.0f));
    std::vector<phong_poly_t> polys(faceLightmaps.size());

    for (size_t f = 0; f < faceLightmaps.size(); f++)
    {
        const auto& lm = faceLightmaps[f];
        if (lm.bspFaceIndex < 0 || (g_BSP.GetTexinfo(lm.texinfoIndex).flags & 1) || g_BSP.GetFaceDispIndex(lm.bspFaceIndex) >= 0)
            continue;

        const auto& bspFace = g_BSP.GetFace(lm.bspFaceIndex);
        const auto& pl = g_BSP.GetPlane(bspFace.planenum);

        Float sideSign = (bspFace.side != 0) ? -1.0f : 1.0f;
        polys[f].bspFaceIndex = lm.bspFaceIndex;
        polys[f].normal[0] = pl.normal[0] * sideSign;
        polys[f].normal[1] = pl.normal[1] * sideSign;
        polys[f].normal[2] = pl.normal[2] * sideSign;

        for (Int32 e = 0; e < bspFace.numedges; e++)
        {
            Int32 surfEdge = g_BSP.GetSurfEdge(bspFace.firstedge + e);
            Uint32 vIdx = (surfEdge >= 0) ? g_BSP.GetEdge(surfEdge).vertexes[0] : g_BSP.GetEdge(-surfEdge).vertexes[1];
            const auto& v = g_BSP.GetVertex(vIdx);
            polys[f].verts.push_back({ v.origin[0], v.origin[1], v.origin[2] });
        }
        polys[f].vertNormals.resize(polys[f].verts.size());
    }

    #pragma omp parallel for schedule(dynamic)
    for (int f = 0; f < (int)polys.size(); f++)
    {
        auto& poly = polys[f];
        if (poly.verts.size() < 3)
            continue;

        for (size_t v = 0; v < poly.verts.size(); v++)
        {
            Float accum[3] = { poly.normal[0], poly.normal[1], poly.normal[2] };
            const auto& p0 = poly.verts[v];

            for (size_t f2 = 0; f2 < polys.size(); f2++)
            {
                if (f == (int)f2 || polys[f2].verts.size() < 3)
                    continue;

                Float dotNorm = poly.normal[0] * polys[f2].normal[0] +
                                poly.normal[1] * polys[f2].normal[1] +
                                poly.normal[2] * polys[f2].normal[2];

                if (dotNorm < minDot)
                    continue;

                for (const auto& p1 : polys[f2].verts)
                {
                    Float dx = p0[0] - p1[0];
                    Float dy = p0[1] - p1[1];
                    Float dz = p0[2] - p1[2];
                    if ((dx * dx + dy * dy + dz * dz) < 0.25f)
                    {
                        accum[0] += polys[f2].normal[0];
                        accum[1] += polys[f2].normal[1];
                        accum[2] += polys[f2].normal[2];
                        break;
                    }
                }
            }

            Float len = sqrtf(accum[0] * accum[0] + accum[1] * accum[1] + accum[2] * accum[2]);
            if (len > 0.0001f)
            {
                poly.vertNormals[v] = { accum[0] / len, accum[1] / len, accum[2] / len };
            }
            else
            {
                poly.vertNormals[v] = { poly.normal[0], poly.normal[1], poly.normal[2] };
            }
        }
    }

    #pragma omp parallel for schedule(dynamic)
    for (int f = 0; f < (int)polys.size(); f++)
    {
        const auto& poly = polys[f];
        if (poly.verts.size() < 3)
            continue;

        auto& lm = faceLightmaps[f];
        for (Int32 i = 0; i < lm.totalLuxels; i++)
        {
            auto& coord = lm.sampleCoords[i];
            Float bestDistSq = 1e9f;
            Float bestNormal[3] = { poly.normal[0], poly.normal[1], poly.normal[2] };

            for (size_t t = 1; t + 1 < poly.verts.size(); t++)
            {
                Float triNormal[3];
                Float distSq = 1e9f;

                InterpolateTriangleBarycentric(poly.verts[0], poly.verts[t], poly.verts[t + 1],
                                              poly.vertNormals[0], poly.vertNormals[t], poly.vertNormals[t + 1],
                                              coord.worldPos, triNormal, distSq);

                if (distSq < bestDistSq)
                {
                    bestDistSq = distSq;
                    bestNormal[0] = triNormal[0];
                    bestNormal[1] = triNormal[1];
                    bestNormal[2] = triNormal[2];
                }
            }

            Float len = sqrtf(bestNormal[0] * bestNormal[0] + bestNormal[1] * bestNormal[1] + bestNormal[2] * bestNormal[2]);
            if (len > 0.0001f)
            {
                coord.normal[0] = bestNormal[0] / len;
                coord.normal[1] = bestNormal[1] / len;
                coord.normal[2] = bestNormal[2] / len;
            }
        }
    }
}