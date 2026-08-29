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
#include "datatypes.h"
#include "mapparser.h"
#include "mapparser_lexer.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cmath>

static Char* ReadEntireFile(const Char* filename, size_t* pOutSize)
{
    FILE* f = fopen(filename, "rb");
    if (!f)
    {
        return nullptr;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0)
    {
        fclose(f);
        return nullptr;
    }

    Char* buffer = (Char*)malloc(size + 1);
    if (!buffer)
    {
        fclose(f);
        return nullptr;
    }

    size_t bytesRead = fread(buffer, 1, size, f);
    buffer[bytesRead] = '\0';
    fclose(f);

    if (pOutSize)
    {
        *pOutSize = bytesRead;
    }
    return buffer;
}

bool ParseMapFile(const Char* filename, map_data_t& outMap)
{
    size_t length = 0;
    Char* pBuffer = ReadEntireFile(filename, &length);
    if (!pBuffer)
    {
        return false;
    }

    CMapLexer lexer(pBuffer, length);
    Char token[256];

    while (lexer.NextToken(token, sizeof(token)))
    {
        if (strcmp(token, "{") != 0)
        {
            continue;
        }

        map_entity_t entity;

        while (lexer.NextToken(token, sizeof(token)))
        {
            if (strcmp(token, "}") == 0)
            {
                break;
            }

            if (strcmp(token, "{") == 0)
            {
                map_brush_t brush;

                while (lexer.NextToken(token, sizeof(token)))
                {
                    if (strcmp(token, "}") == 0)
                    {
                        break;
                    }

                    if (strcmp(token, "(") != 0)
                    {
                        continue;
                    }

                    map_brushside_t side;
                    memset(&side, 0, sizeof(side));
                    side.face_id = -1;

                    for (Int32 i = 0; i < 3; i++)
                    {
                        if (i > 0)
                        {
                            lexer.NextToken(token, sizeof(token));
                        }
                        for (Int32 j = 0; j < 3; j++)
                        {
                            lexer.NextToken(token, sizeof(token));
                            side.planepts[i][j] = (Float)atof(token);
                        }
                        lexer.NextToken(token, sizeof(token));
                    }

                    lexer.NextToken(token, sizeof(token));
                    for (size_t i = 0; token[i]; i++)
                    {
                        token[i] = (Char)toupper((unsigned char)token[i]);
                    }
                    strncpy(side.texture, token, sizeof(side.texture) - 1);

                    lexer.NextToken(token, sizeof(token));
                    for (Int32 j = 0; j < 4; j++)
                    {
                        lexer.NextToken(token, sizeof(token));
                        side.uaxis[j] = (Float)atof(token);
                    }
                    lexer.NextToken(token, sizeof(token));

                    lexer.NextToken(token, sizeof(token));
                    for (Int32 j = 0; j < 4; j++)
                    {
                        lexer.NextToken(token, sizeof(token));
                        side.vaxis[j] = (Float)atof(token);
                    }
                    lexer.NextToken(token, sizeof(token));

                    lexer.NextToken(token, sizeof(token));
                    side.rotation = (Float)atof(token);

                    lexer.NextToken(token, sizeof(token));
                    side.scale[0] = (Float)atof(token);
                    lexer.NextToken(token, sizeof(token));
                    side.scale[1] = (Float)atof(token);

                    while (lexer.NextToken(token, sizeof(token), false))
                    {
                        if (strcmp(token, "face_id") == 0)
                        {
                            lexer.NextToken(token, sizeof(token), false);
                            side.face_id = atoi(token);
                        }
                    }

                    brush.sides.push_back(side);
                }

                entity.brushes.push_back(brush);
            }
            else
            {
                map_epair_t epair;
                epair.key = token;
                lexer.NextToken(token, sizeof(token));
                epair.value = token;
                entity.epairs.push_back(epair);
            }
        }

        outMap.entities.push_back(entity);
    }

    free(pBuffer);
    return true;
}

bool ParseMapDisp(const Char* filename, map_disp_data_t& outDisp)
{
    size_t length = 0;
    Char* pBuffer = ReadEntireFile(filename, &length);
    if (!pBuffer)
    {
        return false;
    }

    CMapLexer lexer(pBuffer, length);
    Char token[256];

    while (lexer.NextToken(token, sizeof(token)))
    {
        if (strcmp(token, "{") != 0)
        {
            continue;
        }

        map_dispinfo_t disp;
        memset(&disp, 0, sizeof(disp));
        disp.face_id = -1;
        disp.power = 0;
        strcpy(disp.texture2, "NULL");

        std::vector<Float> distances;
        std::vector<Float> vectors;
        std::vector<Float> alphas;
        Float start_position[3] = { 0.0f, 0.0f, 0.0f };

        while (lexer.NextToken(token, sizeof(token)))
        {
            if (strcmp(token, "}") == 0)
            {
                break;
            }

            if (strcmp(token, "face_id") == 0)
            {
                lexer.NextToken(token, sizeof(token));
                disp.face_id = atoi(token);
            }
            else if (strcmp(token, "power") == 0)
            {
                lexer.NextToken(token, sizeof(token));
                disp.power = atoi(token);
            }
            else if (strcmp(token, "texture2") == 0)
            {
                lexer.NextToken(token, sizeof(token));
                strncpy(disp.texture2, token, sizeof(disp.texture2) - 1);
            }
            else if (strcmp(token, "start_position") == 0)
            {
                for (Int32 i = 0; i < 3; i++)
                {
                    lexer.NextToken(token, sizeof(token));
                    start_position[i] = (Float)atof(token);
                }
            }
            else if (strcmp(token, "corners") == 0)
            {
                for (Int32 c = 0; c < 4; c++)
                {
                    for (Int32 i = 0; i < 3; i++)
                    {
                        lexer.NextToken(token, sizeof(token));
                        disp.corners[c][i] = (Float)atof(token);
                    }
                }
            }
            else if (strcmp(token, "distances") == 0)
            {
                Int32 count = (1 << disp.power) + 1;
                count *= count;
                distances.reserve(count);
                for (Int32 i = 0; i < count; i++)
                {
                    lexer.NextToken(token, sizeof(token));
                    distances.push_back((Float)atof(token));
                }
            }
            else if (strcmp(token, "vectors") == 0)
            {
                Int32 count = (1 << disp.power) + 1;
                count = count * count * 3;
                vectors.reserve(count);
                for (Int32 i = 0; i < count; i++)
                {
                    lexer.NextToken(token, sizeof(token));
                    vectors.push_back((Float)atof(token));
                }
            }
            else if (strcmp(token, "alphas") == 0)
            {
                Int32 count = (1 << disp.power) + 1;
                count *= count;
                alphas.reserve(count);
                for (Int32 i = 0; i < count; i++)
                {
                    lexer.NextToken(token, sizeof(token));
                    alphas.push_back((Float)atof(token));
                }
            }
        }

        Int32 vertCount = (1 << disp.power) + 1;
        vertCount *= vertCount;
        disp.verts.resize(vertCount);

        for (Int32 i = 0; i < vertCount; i++)
        {
            if (i < (Int32)distances.size())
            {
                disp.verts[i].distance = distances[i];
            }
            if (i < (Int32)alphas.size())
            {
                disp.verts[i].alpha = alphas[i];
            }
            if (i * 3 + 2 < (Int32)vectors.size())
            {
                disp.verts[i].vector[0] = vectors[i * 3 + 0];
                disp.verts[i].vector[1] = vectors[i * 3 + 1];
                disp.verts[i].vector[2] = vectors[i * 3 + 2];
            }
        }

        Int32 start_index = 0;
        Float min_dist = 99999999.0f;
        for (Int32 c = 0; c < 4; c++)
        {
            Float dx = disp.corners[c][0] - start_position[0];
            Float dy = disp.corners[c][1] - start_position[1];
            Float dz = disp.corners[c][2] - start_position[2];
            Float dist = sqrtf(dx * dx + dy * dy + dz * dz);
            if (dist < min_dist)
            {
                min_dist = dist;
                start_index = c;
            }
        }

        if (start_index != 0)
        {
            Float temp_corners[4][3];
            for (Int32 c = 0; c < 4; c++)
            {
                temp_corners[c][0] = disp.corners[(start_index + c) % 4][0];
                temp_corners[c][1] = disp.corners[(start_index + c) % 4][1];
                temp_corners[c][2] = disp.corners[(start_index + c) % 4][2];
            }
            for (Int32 c = 0; c < 4; c++)
            {
                disp.corners[c][0] = temp_corners[c][0];
                disp.corners[c][1] = temp_corners[c][1];
                disp.corners[c][2] = temp_corners[c][2];
            }

            Int32 N = 1 << disp.power;
            Int32 K = N + 1;
            std::vector<map_dispvert_t> temp_verts = disp.verts;

            for (Int32 v = 0; v < K; v++)
            {
                for (Int32 u = 0; u < K; u++)
                {
                    Int32 src_u = u;
                    Int32 src_v = v;
                    if (start_index == 1)
                    {
                        src_u = N - v;
                        src_v = u;
                    }
                    else if (start_index == 2)
                    {
                        src_u = N - u;
                        src_v = N - v;
                    }
                    else if (start_index == 3)
                    {
                        src_u = v;
                        src_v = N - u;
                    }
                    disp.verts[v * K + u] = temp_verts[src_v * K + src_u];
                }
            }
        }

        outDisp.displacements.push_back(disp);
    }

    free(pBuffer);
    return true;
}