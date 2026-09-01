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
#include "rad.h"
#include "bsp.h"
#include <cmath>
#include <cstring>
#include <algorithm>

void CRadPipeline::AddSunLight(const map_entity_t& ent, Int32 style)
{
    rad_light_t l;
    memset(&l, 0, sizeof(l));
    l.type = LIGHT_SUN;

    Float pitch = (Float)atof(ent.GetValue("pitch"));
    Float angles[3] = { 0.0f, 0.0f, 0.0f };
    sscanf(ent.GetValue("angles"), "%f %f %f", &angles[0], &angles[1], &angles[2]);
    if (pitch != 0.0f)
    {
        angles[0] = pitch;
    }

    l.style = style;

    Float p = angles[0] * (M_PI / 180.0f);
    Float y = angles[1] * (M_PI / 180.0f);

    l.normal[0] = cosf(p) * cosf(y);
    l.normal[1] = cosf(p) * sinf(y);
    l.normal[2] = sinf(p);

    Float r = 255.0f, g = 255.0f, b = 255.0f, brightness = 200.0f;
    sscanf(ent.GetValue("_light"), "%f %f %f %f", &r, &g, &b, &brightness);

    l.color[0] = (r / 255.0f) * brightness;
    l.color[1] = (g / 255.0f) * brightness;
    l.color[2] = (b / 255.0f) * brightness;

    m_lights.push_back(l);
}
void CRadPipeline::AddPointLight(const map_entity_t& ent, Int32 style)
{
    rad_light_t l;
    memset(&l, 0, sizeof(l));
    l.type = LIGHT_POINT;

    sscanf(ent.GetValue("origin"), "%f %f %f", &l.origin[0], &l.origin[1], &l.origin[2]);

    Float r = 255.0f, g = 255.0f, b = 255.0f, brightness = 200.0f;
    sscanf(ent.GetValue("_light"), "%f %f %f %f", &r, &g, &b, &brightness);

    Float fade = (Float)atof(ent.GetValue("_fade"));
    l.fade = (fade <= 0.0f) ? 1.0f : fade;
    l.style = style;
    l.falloff = atoi(ent.GetValue("_falloff"));

    Float scaleR = (r / 255.0f) * brightness;
    Float scaleG = (g / 255.0f) * brightness;
    Float scaleB = (b / 255.0f) * brightness;
    Float maxVal = std::max({ scaleR, scaleG, scaleB });
    Float l1 = maxVal * maxVal / 10.0f;
    l.color[0] = scaleR * l1;
    l.color[1] = scaleG * l1;
    l.color[2] = scaleB * l1;

    m_lights.push_back(l);
}
void CRadPipeline::AddSpotLight(const map_entity_t& ent, Int32 style)
{
    rad_light_t l;
    memset(&l, 0, sizeof(l));
    l.type = LIGHT_SPOT;

    sscanf(ent.GetValue("origin"), "%f %f %f", &l.origin[0], &l.origin[1], &l.origin[2]);

    Float pitch = (Float)atof(ent.GetValue("pitch"));
    Float angles[3] = { 0.0f, 0.0f, 0.0f };
    sscanf(ent.GetValue("angles"), "%f %f %f", &angles[0], &angles[1], &angles[2]);
    if (pitch != 0.0f)
    {
        angles[0] = pitch;
    }

    l.style = style;
    l.falloff = atoi(ent.GetValue("_falloff"));

    Float p = angles[0] * (M_PI / 180.0f);
    Float y = angles[1] * (M_PI / 180.0f);

    l.normal[0] = cosf(p) * cosf(y);
    l.normal[1] = cosf(p) * sinf(y);
    l.normal[2] = sinf(p);

    Float r = 255.0f, g = 255.0f, b = 255.0f, brightness = 200.0f;
    sscanf(ent.GetValue("_light"), "%f %f %f %f", &r, &g, &b, &brightness);

    Float cone1 = (Float)atof(ent.GetValue("_cone"));
    Float cone2 = (Float)atof(ent.GetValue("_cone2"));
    if (cone1 <= 0.0f) 
        cone1 = 10.0f;
    if (cone2 <= 0.0f) 
        cone2 = cone1;

    l.stopdot = cosf(cone1 * (M_PI / 180.0f));
    l.stopdot2 = cosf(cone2 * (M_PI / 180.0f));

    Float fade = (Float)atof(ent.GetValue("_fade"));
    l.fade = (fade <= 0.0f) ? 1.0f : fade;

    Float scaleR = (r / 255.0f) * brightness;
    Float scaleG = (g / 255.0f) * brightness;
    Float scaleB = (b / 255.0f) * brightness;
    Float maxVal = std::max({ scaleR, scaleG, scaleB });
    Float l1 = maxVal * maxVal / 10.0f;
    l.color[0] = scaleR * l1;
    l.color[1] = scaleG * l1;
    l.color[2] = scaleB * l1;

    m_lights.push_back(l);
}

void CRadPipeline::AlphaTestFilterCallback(const struct RTCFilterFunctionNArguments* args)
{
    CRadPipeline* pipeline = reinterpret_cast<CRadPipeline*>(args->geometryUserPtr);
    if (!pipeline)
        return;

    for (unsigned int i = 0; i < args->N; i++)
    {
        if (args->valid[i] == 0)
            continue;

        unsigned int primID = RTCHitN_primID(args->hit, args->N, i);
        if (primID >= pipeline->m_scenePrims.size())
            continue;

        const scene_prim_t& prim = pipeline->m_scenePrims[primID];
        if (prim.faceIndex < 0 || prim.faceIndex >= (Int32)pipeline->m_faceInfos.size())
            continue;

        const face_info_t& fInfo = pipeline->m_faceInfos[prim.faceIndex];
        if (!fInfo.hasAlphaTest || !fInfo.diffuseImage || fInfo.diffuseImage->rgba.empty())
            continue;

        float u = RTCHitN_u(args->hit, args->N, i);
        float v = RTCHitN_v(args->hit, args->N, i);
        float w = 1.0f - u - v;

        float texU = w * prim.uv[0][0] + u * prim.uv[1][0] + v * prim.uv[2][0];
        float texV = w * prim.uv[0][1] + u * prim.uv[1][1] + v * prim.uv[2][1];

        texU = texU - floorf(texU);
        texV = texV - floorf(texV);

        int px = std::clamp((int)(texU * fInfo.diffuseImage->width), 0, fInfo.diffuseImage->width - 1);
        int py = std::clamp((int)(texV * fInfo.diffuseImage->height), 0, fInfo.diffuseImage->height - 1);

        size_t pixelOffset = ((size_t)py * fInfo.diffuseImage->width + px) * 4;
        byte alpha = fInfo.diffuseImage->rgba[pixelOffset + 3];

        if (alpha < 128)
        {
            args->valid[i] = 0;
        }
    }
}

void CRadPipeline::ParseLights(map_data_t& mapData, const std::string& daystage)
{
    m_lights.clear();

    std::unordered_map<std::string, Int32> targetnameStyles;
    Int32 nextAllocatedStyle = 32;

    for (auto& ent : mapData.entities)
    {
        const Char* classname = ent.GetValue("classname");
        bool isNightOnly = (atoi(ent.GetValue("nightmode")) == 1);
        bool isDaylightReturnOnly = (atoi(ent.GetValue("daylightreturn")) == 1);

        if (daystage == "nightmode")
        {
            if (isDaylightReturnOnly)
                continue;
            if (!strcmp(classname, "light_environment") && !isNightOnly)
                continue;
        }
        else if (daystage == "daylightreturn")
        {
            if (isNightOnly)
                continue;
            if (!strcmp(classname, "light_environment") && !isDaylightReturnOnly)
                continue;
        }
        else
        {
            if (isNightOnly || isDaylightReturnOnly)
                continue;
        }

        Int32 style = atoi(ent.GetValue("style"));
        const Char* targetname = ent.GetValue("targetname");
        if (!targetname[0])
            targetname = ent.GetValue("name");

        if (targetname[0])
        {
            if (style == 0)
            {
                auto it = targetnameStyles.find(targetname);
                if (it != targetnameStyles.end())
                {
                    style = it->second;
                }
                else
                {
                    style = nextAllocatedStyle++;
                    targetnameStyles[targetname] = style;
                }

                bool foundStyleKey = false;
                for (auto& ep : ent.epairs)
                {
                    if (ep.key == "style")
                    {
                        ep.value = std::to_string(style);
                        foundStyleKey = true;
                        break;
                    }
                }
                if (!foundStyleKey)
                {
                    ent.epairs.push_back({ "style", std::to_string(style) });
                }
            }
        }

        if (!strcmp(classname, "light_environment"))
        {
            AddSunLight(ent, style);
        }
        else if (!strcmp(classname, "light") || !strcmp(classname, "night_light"))
        {
            AddPointLight(ent, style);
        }
        else if (!strcmp(classname, "light_spot") || !strcmp(classname, "night_light_spot"))
        {
            AddSpotLight(ent, style);
        }
    }
}