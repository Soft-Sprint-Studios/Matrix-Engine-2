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
#ifndef BRUSH_H
#define BRUSH_H

#include "datatypes.h"
#include "mapparser.h"
#include "bsp_tree.h"
#include <vector>
#include <string>

struct lightmap_face_t;

struct poly_vert_t
{
    Float pos[3];
};

struct poly_face_t
{
    Int32 planeIndex;
    Int32 texinfoIndex;
    Int32 face_id;
    Float normal[3];
    Float dist;
    std::vector<poly_vert_t> verts;
};

struct poly_brush_t
{
    std::vector<poly_face_t> faces;
    Float mins[3];
    Float maxs[3];
};

bool BuildBrushPolygons(const map_brush_t& inBrush, poly_brush_t& outPoly);
bool ProcessMapGeometry(map_data_t& mapData, const map_disp_data_t& dispData, std::vector<lightmap_face_t>& outFaceLightmaps);
bool SplitPolygonByPlane(const std::vector<poly_vert_t>& inVerts, const Float normal[3], Float dist, std::vector<poly_vert_t>& outFront, std::vector<poly_vert_t>& outBack);
std::string SerializeEntities(const map_data_t& mapData);

#endif // BRUSH_H