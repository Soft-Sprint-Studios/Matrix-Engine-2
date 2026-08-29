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
#include "pmf.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <string>

static const Char* LexToken(const Char* str, Char* outToken, size_t maxLen)
{
    outToken[0] = '\0';
    if (!str)
    {
        return nullptr;
    }

    while (*str)
    {
        while (*str && isspace((unsigned char)*str))
        {
            str++;
        }

        if (!*str)
        {
            return nullptr;
        }

        if (*str == '/' && *(str + 1) == '/')
        {
            while (*str && *str != '\n')
            {
                str++;
            }
            continue;
        }

        if (*str == '/' && *(str + 1) == '*')
        {
            str += 2;
            while (*str && !(*str == '*' && *(str + 1) == '/'))
            {
                str++;
            }
            if (*str)
            {
                str += 2;
            }
            continue;
        }

        break;
    }

    if (!*str)
    {
        return nullptr;
    }

    if (*str == '{' || *str == '}')
    {
        outToken[0] = *str;
        outToken[1] = '\0';
        return str + 1;
    }

    size_t i = 0;
    if (*str == '"')
    {
        str++;
        while (*str && *str != '"' && *str != '\n')
        {
            if (i < maxLen - 1)
            {
                outToken[i++] = *str;
            }
            str++;
        }
        if (*str == '"')
        {
            str++;
        }
    }
    else
    {
        while (*str && !isspace((unsigned char)*str) && *str != '{' && *str != '}' && *str != '"')
        {
            if (i < maxLen - 1)
            {
                outToken[i++] = *str;
            }
            str++;
        }
    }

    outToken[i] = '\0';
    return str;
}

static bool ParsePMFScript(const Char* scriptBuffer, material_t& outMat, std::string& outAliasScript)
{
    const Char* ptr = scriptBuffer;
    Char token[256];
    bool isAlias = false;

    while ((ptr = LexToken(ptr, token, sizeof(token))) != nullptr)
    {
        if (token[0] == '\0' || token[0] == '{' || token[0] == '}')
        {
            continue;
        }

        std::string keyword(token);
        std::transform(keyword.begin(), keyword.end(), keyword.begin(), [](unsigned char c) {
            return std::tolower(c);
            });

        if (keyword == "$alias")
        {
            isAlias = true;
        }
        else if (isAlias && keyword == "$scriptfile")
        {
            ptr = LexToken(ptr, token, sizeof(token));
            if (ptr && token[0])
            {
                outAliasScript = token;
                return true;
            }
        }
        else if (keyword == "$texture")
        {
            ptr = LexToken(ptr, token, sizeof(token));
            if (ptr && token[0])
            {
                std::string subKeyword(token);
                std::transform(subKeyword.begin(), subKeyword.end(), subKeyword.begin(), [](unsigned char c) {
                    return std::tolower(c);
                    });

                if (subKeyword == "diffuse")
                {
                    ptr = LexToken(ptr, token, sizeof(token));
                    if (ptr && token[0])
                    {
                        outMat.diffusePath = token;
                    }
                }
            }
        }
        else if (keyword == "$alphatest")
        {
            outMat.hasAlphaTest = true;
        }
        else if (keyword == "$noradshadows" || keyword == "$noshadow")
        {
            outMat.hasNoShadow = true;
        }
        else if (keyword == "$texreflectscale")
        {
            ptr = LexToken(ptr, token, sizeof(token));
            if (ptr && token[0])
            {
                outMat.reflectScale = (Float)atof(token);
            }
        }
    }

    return !outMat.diffusePath.empty();
}

bool LoadMaterial(const Char* baseDir, const Char* materialName, material_t& outMat)
{
    outMat.hasAlphaTest = false;
    outMat.hasNoShadow = false;
    outMat.reflectScale = 1.0f;
    outMat.scriptPath = materialName;

    Char pmfPath[512];
    snprintf(pmfPath, sizeof(pmfPath), "%s/textures/%s.pmf", baseDir, materialName);

    FILE* f = fopen(pmfPath, "rb");
    if (!f)
    {
        snprintf(pmfPath, sizeof(pmfPath), "%s/%s.pmf", baseDir, materialName);
        f = fopen(pmfPath, "rb");
    }

    if (!f)
    {
        Char ddsPath[512];
        snprintf(ddsPath, sizeof(ddsPath), "%s/textures/%s.dds", baseDir, materialName);
        if (LoadDDSFromFile(ddsPath, outMat.diffuseImage))
        {
            outMat.diffusePath = ddsPath;
            return true;
        }
        return false;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::vector<Char> buf(sz + 1, 0);
    fread(buf.data(), 1, sz, f);
    fclose(f);

    std::string alias;
    if (!ParsePMFScript(buf.data(), outMat, alias))
    {
        if (!alias.empty())
        {
            return LoadMaterial(baseDir, alias.c_str(), outMat);
        }
        return false;
    }

    Char ddsPath[512];
    snprintf(ddsPath, sizeof(ddsPath), "%s/%s", baseDir, outMat.diffusePath.c_str());
    if (!LoadDDSFromFile(ddsPath, outMat.diffuseImage))
    {
        snprintf(ddsPath, sizeof(ddsPath), "%s/textures/%s", baseDir, outMat.diffusePath.c_str());
        if (!LoadDDSFromFile(ddsPath, outMat.diffuseImage))
        {
            return false;
        }
    }

    return true;
}