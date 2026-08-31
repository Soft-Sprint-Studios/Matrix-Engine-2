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
#include "brush.h"
#include "bsp.h"
#include <cmath>
#include <cstring>
#include <algorithm>

static constexpr Float EPSILON_PLANE = 0.001f;

static void PlaneFromPoints(const Float p0[3], const Float p1[3], const Float p2[3], Float outNormal[3], Float& outDist)
{
    Float d0[3] = { p0[0] - p1[0], p0[1] - p1[1], p0[2] - p1[2] };
    Float d1[3] = { p2[0] - p1[0], p2[1] - p1[1], p2[2] - p1[2] };

    outNormal[0] = d0[1] * d1[2] - d0[2] * d1[1];
    outNormal[1] = d0[2] * d1[0] - d0[0] * d1[2];
    outNormal[2] = d0[0] * d1[1] - d0[1] * d1[0];

    Float length = sqrtf(outNormal[0] * outNormal[0] + outNormal[1] * outNormal[1] + outNormal[2] * outNormal[2]);
    if (length > 0.00001f)
    {
        outNormal[0] /= length;
        outNormal[1] /= length;
        outNormal[2] /= length;
    }
    else
    {
        outNormal[0] = 0.0f;
        outNormal[1] = 0.0f;
        outNormal[2] = 1.0f;
    }

    outDist = p0[0] * outNormal[0] + p0[1] * outNormal[1] + p0[2] * outNormal[2];
}

static std::vector<poly_vert_t> MakePlanePolygon(const Float normal[3], Float dist)
{
    Int32 axis = 0;
    Float maxVal = -1.0f;

    for (Int32 i = 0; i < 3; i++)
    {
        Float v = fabsf(normal[i]);
        if (v > maxVal)
        {
            maxVal = v;
            axis = i;
        }
    }

    Float up[3] = { 0.0f, 0.0f, 0.0f };
    if (axis == 0 || axis == 1)
    {
        up[2] = 1.0f;
    }
    else
    {
        up[0] = 1.0f;
    }

    Float d = up[0] * normal[0] + up[1] * normal[1] + up[2] * normal[2];
    up[0] -= d * normal[0];
    up[1] -= d * normal[1];
    up[2] -= d * normal[2];

    Float len = sqrtf(up[0] * up[0] + up[1] * up[1] + up[2] * up[2]);
    up[0] /= len;
    up[1] /= len;
    up[2] /= len;

    Float right[3];
    right[0] = up[1] * normal[2] - up[2] * normal[1];
    right[1] = up[2] * normal[0] - up[0] * normal[2];
    right[2] = up[0] * normal[1] - up[1] * normal[0];

    Float center[3] = { normal[0] * dist, normal[1] * dist, normal[2] * dist };
    Float extent = 131072.0f;

    std::vector<poly_vert_t> poly(4);
    for (Int32 i = 0; i < 3; i++)
    {
        poly[0].pos[i] = center[i] - (right[i] * extent) + (up[i] * extent);
        poly[1].pos[i] = center[i] + (right[i] * extent) + (up[i] * extent);
        poly[2].pos[i] = center[i] + (right[i] * extent) - (up[i] * extent);
        poly[3].pos[i] = center[i] - (right[i] * extent) - (up[i] * extent);
    }

    return poly;
}

static bool ClipPolygonByPlane(std::vector<poly_vert_t>& inOutVerts, const Float normal[3], Float dist)
{
    size_t count = inOutVerts.size();
    if (count < 3)
    {
        return false;
    }

    std::vector<Float> distances(count);
    std::vector<Int32> sides(count);
    Int32 frontCount = 0;
    Int32 backCount = 0;

    for (size_t i = 0; i < count; i++)
    {
        Float d = (inOutVerts[i].pos[0] * normal[0] + inOutVerts[i].pos[1] * normal[1] + inOutVerts[i].pos[2] * normal[2]) - dist;
        distances[i] = d;

        if (d > EPSILON_PLANE)
        {
            sides[i] = 0;
            frontCount++;
        }
        else if (d < -EPSILON_PLANE)
        {
            sides[i] = 1;
            backCount++;
        }
        else
        {
            sides[i] = 2;
        }
    }

    if (frontCount == 0)
    {
        inOutVerts.clear();
        return false;
    }

    if (backCount == 0)
    {
        return true;
    }

    std::vector<poly_vert_t> output;

    for (size_t i = 0; i < count; i++)
    {
        size_t next = (i + 1) % count;

        if (sides[i] == 0 || sides[i] == 2)
        {
            output.push_back(inOutVerts[i]);
        }

        if ((sides[i] == 0 && sides[next] == 1) || (sides[i] == 1 && sides[next] == 0))
        {
            Float frac = distances[i] / (distances[i] - distances[next]);
            poly_vert_t mid;
            for (Int32 k = 0; k < 3; k++)
            {
                mid.pos[k] = inOutVerts[i].pos[k] + frac * (inOutVerts[next].pos[k] - inOutVerts[i].pos[k]);
            }
            output.push_back(mid);
        }
    }

    inOutVerts = output;
    return inOutVerts.size() >= 3;
}

bool BuildBrushPolygons(const map_brush_t& inBrush, poly_brush_t& outPoly)
{
    size_t numSides = inBrush.sides.size();
    if (numSides < 4)
    {
        return false;
    }

    struct brush_plane_t
    {
        Float normal[3];
        Float dist;
    };

    std::vector<brush_plane_t> planes(numSides);
    for (size_t i = 0; i < numSides; i++)
    {
        PlaneFromPoints(inBrush.sides[i].planepts[0], inBrush.sides[i].planepts[1], inBrush.sides[i].planepts[2], planes[i].normal, planes[i].dist);
    }

    outPoly.mins[0] = outPoly.mins[1] = outPoly.mins[2] = 9999999.0f;
    outPoly.maxs[0] = outPoly.maxs[1] = outPoly.maxs[2] = -9999999.0f;

    for (size_t i = 0; i < numSides; i++)
    {
        std::vector<poly_vert_t> polygon = MakePlanePolygon(planes[i].normal, planes[i].dist);

        for (size_t j = 0; j < numSides; j++)
        {
            if (i == j)
            {
                continue;
            }

            Float clipNormal[3] = { -planes[j].normal[0], -planes[j].normal[1], -planes[j].normal[2] };
            Float clipDist = -planes[j].dist;

            if (!ClipPolygonByPlane(polygon, clipNormal, clipDist))
            {
                break;
            }
        }

        if (polygon.size() < 3)
        {
            continue;
        }

        poly_face_t face;
        face.normal[0] = planes[i].normal[0];
        face.normal[1] = planes[i].normal[1];
        face.normal[2] = planes[i].normal[2];
        face.dist = planes[i].dist;
        face.face_id = inBrush.sides[i].face_id;

        Int32 axisType = 3;
        if (face.normal[0] > 0.999f) 
            axisType = 0;
        else if (face.normal[1] > 0.999f) 
            axisType = 1;
        else if (face.normal[2] > 0.999f) 
            axisType = 2;
        else
        {
            Float ax = fabsf(face.normal[0]);
            Float ay = fabsf(face.normal[1]);
            Float az = fabsf(face.normal[2]);
            if (ax >= ay && ax >= az) 
                axisType = 3;
            else if (ay >= ax && ay >= az) 
                axisType = 4;
            else 
                axisType = 5;
        }

        face.planeIndex = g_BSP.InsertPlane(face.normal, face.dist, axisType);

        Int32 textureIdx = g_BSP.InsertTexture(inBrush.sides[i].texture);

        Float vecs[2][4];
        Float scaleU = inBrush.sides[i].scale[0] != 0.0f ? inBrush.sides[i].scale[0] : 1.0f;
        Float scaleV = inBrush.sides[i].scale[1] != 0.0f ? inBrush.sides[i].scale[1] : 1.0f;

        for (Int32 k = 0; k < 4; k++)
        {
            vecs[0][k] = inBrush.sides[i].uaxis[k] / scaleU;
            vecs[1][k] = inBrush.sides[i].vaxis[k] / scaleV;
        }

        Int32 flags = 0;
        if (!strncmp(inBrush.sides[i].texture, "SKY", 3) ||
            !strncmp(inBrush.sides[i].texture, "NULL", 4) ||
            !strncmp(inBrush.sides[i].texture, "ORIGIN", 6) ||
            !strncmp(inBrush.sides[i].texture, "TRIGGER", 7))
        {
            flags |= 1;
        }

        face.texinfoIndex = g_BSP.InsertTexinfo(vecs, textureIdx, flags);
        face.verts = polygon;

        for (const auto& v : polygon)
        {
            for (Int32 k = 0; k < 3; k++)
            {
                if (v.pos[k] < outPoly.mins[k]) 
                    outPoly.mins[k] = v.pos[k];
                if (v.pos[k] > outPoly.maxs[k]) 
                    outPoly.maxs[k] = v.pos[k];
            }
        }

        outPoly.faces.push_back(face);
    }

    return !outPoly.faces.empty();
}