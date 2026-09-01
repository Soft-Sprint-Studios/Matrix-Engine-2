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
#ifndef MAPPARSER_H
#define MAPPARSER_H

#include <vector>
#include <string>
#include "mbspv1file.h"

struct map_epair_t
{
    std::string key;
    std::string value;
};

struct map_brushside_t
{
    Float planepts[3][3];
    Char texture[64];
    Float uaxis[4];
    Float vaxis[4];
    Float rotation;
    Float scale[2];
    Int32 face_id;
};

struct map_brush_t
{
    std::vector<map_brushside_t> sides;
};

struct map_entity_t
{
    std::vector<map_epair_t> epairs;
    std::vector<map_brush_t> brushes;

    const Char* GetValue(const Char* key) const
    {
        for (const auto& ep : epairs)
        {
            if (ep.key == key)
            {
                return ep.value.c_str();
            }
        }
        return "";
    }
};

struct map_data_t
{
    std::vector<map_entity_t> entities;
};

struct map_dispvert_t
{
    Float vector[3];
    Float distance;
    Float alpha;
};

struct map_dispinfo_t
{
    Int32 face_id;
    Int32 power;
    Char texture2[64];
    Float corners[4][3];
    std::vector<map_dispvert_t> verts;
};

struct map_disp_data_t
{
    std::vector<map_dispinfo_t> displacements;
};

bool ParseMapFile(const Char* filename, map_data_t& outMap);
bool ParseMapDisp(const Char* filename, map_disp_data_t& outDisp);

#endif // MAPPARSER_H