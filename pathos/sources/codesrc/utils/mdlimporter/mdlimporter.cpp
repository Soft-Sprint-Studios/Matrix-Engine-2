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
#define CGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "cgltf.h"
#include "stb_image.h"
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <map>
#include <iomanip>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <set>

namespace fs = std::filesystem;

enum class ExportMode
{
    Model,
    Texture
};

struct AppConfig
{
#ifdef _WIN32
    std::string nvcompress = "nvcompress.exe";
    std::string target = "sources\\progs";
    std::string game = "pathos";
#else
    std::string nvcompress = "nvcompress";
    std::string target = "sources/progs";
    std::string game = "pathos";
#endif
};

AppConfig g_Config;

struct BoneTransform
{
    float pos[3]{ 0.0f, 0.0f, 0.0f };
    float rot[3]{ 0.0f, 0.0f, 0.0f };
};

struct AnimationSequenceInfo
{
    std::string name;
    std::string smdName;
    float fps = 30.0f;
    int numFrames = 1;
};

//=============================================
// @brief
//
//=============================================
void LoadConfig()
{
    std::ifstream file("config.txt");
    if (!file.is_open())
    {
        std::ofstream outfile("config.txt");
        outfile << "nvcompress=" << g_Config.nvcompress << "\n";
        outfile << "target=" << g_Config.target << "\n";
        outfile << "game=" << g_Config.game << "\n";
        outfile.close();
        return;
    }

    std::string line;
    while (std::getline(file, line))
    {
        size_t sep = line.find('=');
        if (sep == std::string::npos)
        {
            continue;
        }
        std::string key = line.substr(0, sep);
        std::string val = line.substr(sep + 1);
        if (key == "nvcompress")
        {
            g_Config.nvcompress = val;
        }
        if (key == "target")
        {
            g_Config.target = val;
        }
        if (key == "game")
        {
            g_Config.game = val;
        }
    }
}

#pragma pack(push, 1)
struct BMPFileHeader
{
    uint16_t bfType{ 0x4D42 };
    uint32_t bfSize{ 0 };
    uint16_t bfReserved1{ 0 };
    uint16_t bfReserved2{ 0 };
    uint32_t bfOffBits{ 0 };
};

struct BMPInfoHeader
{
    uint32_t biSize{ 40 };
    int32_t  biWidth{ 0 };
    int32_t  biHeight{ 0 };
    uint16_t biPlanes{ 1 };
    uint16_t biBitCount{ 0 };
    uint32_t biCompression{ 0 };
    uint32_t biSizeImage{ 0 };
    int32_t  biXPelsPerMeter{ 2835 };
    int32_t  biYPelsPerMeter{ 2835 };
    uint32_t biClrUsed{ 0 };
    uint32_t biClrImportant{ 0 };
};
#pragma pack(pop)

struct DecodedImage
{
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels;
};

//=============================================
// @brief
//
//=============================================
DecodedImage LoadGLTFImageRGBA(const cgltf_image* image)
{
    DecodedImage result;
    if (!image || !image->buffer_view || !image->buffer_view->buffer)
    {
        return result;
    }

    const uint8_t* data = static_cast<const uint8_t*>(image->buffer_view->buffer->data) + image->buffer_view->offset;
    size_t size = image->buffer_view->size;
    if (!data || size == 0)
    {
        return result;
    }

    int w = 0, h = 0, comp = 0;
    unsigned char* raw = stbi_load_from_memory(data, static_cast<int>(size), &w, &h, &comp, 4);
    if (!raw)
    {
        return result;
    }

    result.width = w;
    result.height = h;
    result.pixels.assign(raw, raw + (w * h * 4));
    stbi_image_free(raw);
    return result;
}

//=============================================
// @brief
//
//=============================================
std::vector<uint8_t> ResizeRGBA(const uint8_t* src, int srcW, int srcH, int dstW, int dstH)
{
    std::vector<uint8_t> dst(dstW * dstH * 4);
    if (srcW <= 0 || srcH <= 0 || dstW <= 0 || dstH <= 0)
    {
        return dst;
    }

    for (int y = 0; y < dstH; ++y)
    {
        float v = (static_cast<float>(y) + 0.5f) * (static_cast<float>(srcH) / static_cast<float>(dstH)) - 0.5f;
        int y0 = std::clamp(static_cast<int>(std::floor(v)), 0, srcH - 1);
        int y1 = std::clamp(y0 + 1, 0, srcH - 1);
        float fy = v - std::floor(v);

        for (int x = 0; x < dstW; ++x)
        {
            float u = (static_cast<float>(x) + 0.5f) * (static_cast<float>(srcW) / static_cast<float>(dstW)) - 0.5f;
            int x0 = std::clamp(static_cast<int>(std::floor(u)), 0, srcW - 1);
            int x1 = std::clamp(x0 + 1, 0, srcW - 1);
            float fx = u - std::floor(u);

            for (int c = 0; c < 4; ++c)
            {
                float c00 = src[(y0 * srcW + x0) * 4 + c];
                float c10 = src[(y0 * srcW + x1) * 4 + c];
                float c01 = src[(y1 * srcW + x0) * 4 + c];
                float c11 = src[(y1 * srcW + x1) * 4 + c];

                float val = (1.0f - fx) * (1.0f - fy) * c00 +
                            fx * (1.0f - fy) * c10 +
                            (1.0f - fx) * fy * c01 +
                            fx * fy * c11;

                dst[(y * dstW + x) * 4 + c] = static_cast<uint8_t>(std::clamp(val, 0.0f, 255.0f));
            }
        }
    }
    return dst;
}

//=============================================
// @brief
//
//=============================================
bool WriteBMP(const std::string& filename, int w, int h, const uint8_t* rgba, int bpp)
{
    std::ofstream out(filename, std::ios::binary);
    if (!out.is_open())
    {
        return false;
    }

    int bytesPerPixel = bpp / 8;
    int rowStride = w * bytesPerPixel;
    int paddedRowSize = (rowStride + 3) & ~3;
    uint32_t imageSize = paddedRowSize * h;

    BMPFileHeader bfh;
    bfh.bfOffBits = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader);
    bfh.bfSize = bfh.bfOffBits + imageSize;

    BMPInfoHeader bih;
    bih.biWidth = w;
    bih.biHeight = h;
    bih.biBitCount = static_cast<uint16_t>(bpp);
    bih.biSizeImage = imageSize;

    out.write(reinterpret_cast<const char*>(&bfh), sizeof(bfh));
    out.write(reinterpret_cast<const char*>(&bih), sizeof(bih));

    std::vector<uint8_t> rowBuffer(paddedRowSize, 0);

    for (int y = h - 1; y >= 0; --y)
    {
        const uint8_t* srcRow = rgba + (y * w * 4);
        for (int x = 0; x < w; ++x)
        {
            uint8_t r = srcRow[x * 4 + 0];
            uint8_t g = srcRow[x * 4 + 1];
            uint8_t b = srcRow[x * 4 + 2];
            uint8_t a = srcRow[x * 4 + 3];

            if (bpp == 24)
            {
                rowBuffer[x * 3 + 0] = b;
                rowBuffer[x * 3 + 1] = g;
                rowBuffer[x * 3 + 2] = r;
            }
            else
            {
                rowBuffer[x * 4 + 0] = b;
                rowBuffer[x * 4 + 1] = g;
                rowBuffer[x * 4 + 2] = r;
                rowBuffer[x * 4 + 3] = a;
            }
        }
        out.write(reinterpret_cast<const char*>(rowBuffer.data()), paddedRowSize);
    }
    return true;
}

//=============================================
// @brief
//
//=============================================
bool WriteBMP8Indexed(const std::string& filename, int w, int h, const uint8_t* rgba)
{
    std::ofstream out(filename, std::ios::binary);
    if (!out.is_open())
    {
        return false;
    }

    int rowStride = w;
    int paddedRowSize = (rowStride + 3) & ~3;
    uint32_t imageSize = paddedRowSize * h;

    BMPFileHeader bfh;
    bfh.bfOffBits = sizeof(BMPFileHeader) + sizeof(BMPInfoHeader) + (256 * 4);
    bfh.bfSize = bfh.bfOffBits + imageSize;

    BMPInfoHeader bih;
    bih.biWidth = w;
    bih.biHeight = h;
    bih.biBitCount = 8;
    bih.biSizeImage = imageSize;
    bih.biClrUsed = 256;
    bih.biClrImportant = 256;

    out.write(reinterpret_cast<const char*>(&bfh), sizeof(bfh));
    out.write(reinterpret_cast<const char*>(&bih), sizeof(bih));

    uint8_t palette[256 * 4];
    for (int r = 0; r < 6; ++r)
    {
        for (int g = 0; g < 6; ++g)
        {
            for (int b = 0; b < 6; ++b)
            {
                int idx = r * 36 + g * 6 + b;
                palette[idx * 4 + 0] = static_cast<uint8_t>(b * 51);
                palette[idx * 4 + 1] = static_cast<uint8_t>(g * 51);
                palette[idx * 4 + 2] = static_cast<uint8_t>(r * 51);
                palette[idx * 4 + 3] = 0;
            }
        }
    }
    for (int i = 0; i < 40; ++i)
    {
        int idx = 216 + i;
        uint8_t gray = static_cast<uint8_t>((i * 255) / 39);
        palette[idx * 4 + 0] = gray;
        palette[idx * 4 + 1] = gray;
        palette[idx * 4 + 2] = gray;
        palette[idx * 4 + 3] = 0;
    }
    out.write(reinterpret_cast<const char*>(palette), sizeof(palette));

    std::vector<uint8_t> rowBuffer(paddedRowSize, 0);

    for (int y = h - 1; y >= 0; --y)
    {
        const uint8_t* srcRow = rgba + (y * w * 4);
        for (int x = 0; x < w; ++x)
        {
            int r = srcRow[x * 4 + 0];
            int g = srcRow[x * 4 + 1];
            int b = srcRow[x * 4 + 2];

            int r_idx = (r * 5 + 127) / 255;
            int g_idx = (g * 5 + 127) / 255;
            int b_idx = (b * 5 + 127) / 255;
            uint8_t palIdx = static_cast<uint8_t>(r_idx * 36 + g_idx * 6 + b_idx);

            rowBuffer[x] = palIdx;
        }
        out.write(reinterpret_cast<const char*>(rowBuffer.data()), paddedRowSize);
    }
    return true;
}

//=============================================
// @brief
//
//=============================================
bool SaveGLTFImageAsBMP(const cgltf_image* image, const std::string& out_filename, bool is8bppIndexed, int targetW = 0, int targetH = 0, int bpp = 24)
{
    DecodedImage img = LoadGLTFImageRGBA(image);
    if (img.pixels.empty())
    {
        return false;
    }

    int finalW = (targetW > 0) ? targetW : img.width;
    int finalH = (targetH > 0) ? targetH : img.height;

    std::vector<uint8_t> finalPixels;
    if (finalW != img.width || finalH != img.height)
    {
        finalPixels = ResizeRGBA(img.pixels.data(), img.width, img.height, finalW, finalH);
    }
    else
    {
        finalPixels = std::move(img.pixels);
    }

    if (is8bppIndexed)
    {
        return WriteBMP8Indexed(out_filename, finalW, finalH, finalPixels.data());
    }
    else
    {
        return WriteBMP(out_filename, finalW, finalH, finalPixels.data(), bpp);
    }
}

//=============================================
// @brief
//
//=============================================
bool SaveMRAOTexture(const cgltf_material& mat, const std::string& out_filename)
{
    const cgltf_image* mr_image = mat.pbr_metallic_roughness.metallic_roughness_texture.texture ? mat.pbr_metallic_roughness.metallic_roughness_texture.texture->image : nullptr;
    const cgltf_image* occ_image = mat.occlusion_texture.texture ? mat.occlusion_texture.texture->image : nullptr;
    const cgltf_image* em_image = mat.emissive_texture.texture ? mat.emissive_texture.texture->image : nullptr;

    if (!mr_image && !occ_image && !em_image)
    {
        uint8_t roughness = static_cast<uint8_t>(std::clamp(mat.pbr_metallic_roughness.roughness_factor * 255.0f, 0.0f, 255.0f));
        uint8_t metallic = static_cast<uint8_t>(std::clamp(mat.pbr_metallic_roughness.metallic_factor * 255.0f, 0.0f, 255.0f));

        std::vector<uint8_t> solid(16 * 16 * 4);
        for (size_t i = 0; i < 16 * 16; ++i)
        {
            solid[i * 4 + 0] = 255;
            solid[i * 4 + 1] = roughness;
            solid[i * 4 + 2] = metallic;
            solid[i * 4 + 3] = 255;
        }
        return WriteBMP(out_filename, 16, 16, solid.data(), 32);
    }

    DecodedImage mr_img = LoadGLTFImageRGBA(mr_image);
    DecodedImage occ_img = LoadGLTFImageRGBA(occ_image);
    DecodedImage em_img = LoadGLTFImageRGBA(em_image);

    if (mr_img.pixels.empty() && occ_img.pixels.empty() && em_img.pixels.empty())
    {
        return false;
    }

    int width = !mr_img.pixels.empty() ? mr_img.width : (!occ_img.pixels.empty() ? occ_img.width : em_img.width);
    int height = !mr_img.pixels.empty() ? mr_img.height : (!occ_img.pixels.empty() ? occ_img.height : em_img.height);

    std::vector<uint8_t> target(width * height * 4);

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            uint8_t ao = 255;
            uint8_t roughness = 128;
            uint8_t metallic = 0;
            uint8_t alpha = 255;

            if (!mr_img.pixels.empty())
            {
                int mr_x = (x * mr_img.width) / width;
                int mr_y = (y * mr_img.height) / height;
                int idx = (mr_y * mr_img.width + mr_x) * 4;

                ao = mr_img.pixels[idx + 0];
                roughness = mr_img.pixels[idx + 1];
                metallic = mr_img.pixels[idx + 2];
            }

            if (!occ_img.pixels.empty())
            {
                int occ_x = (x * occ_img.width) / width;
                int occ_y = (y * occ_img.height) / height;
                int idx = (occ_y * occ_img.width + occ_x) * 4;

                ao = occ_img.pixels[idx + 0];
            }

            if (!em_img.pixels.empty())
            {
                int em_x = (x * em_img.width) / width;
                int em_y = (y * em_img.height) / height;
                int idx = (em_y * em_img.width + em_x) * 4;

                uint8_t r = em_img.pixels[idx + 0];
                uint8_t g = em_img.pixels[idx + 1];
                uint8_t b = em_img.pixels[idx + 2];
                uint8_t emission = static_cast<uint8_t>((r + g + b) / 3);
                alpha = 255 - emission;
            }

            int targetIdx = (y * width + x) * 4;
            target[targetIdx + 0] = ao;
            target[targetIdx + 1] = roughness;
            target[targetIdx + 2] = metallic;
            target[targetIdx + 3] = alpha;
        }
    }

    return WriteBMP(out_filename, width, height, target.data(), 32);
}

//=============================================
// @brief
//
//=============================================
void ConvertBMPToDDS(const std::string& inputBmp, const std::string& outputDds, const std::string& format = "-bc1 -alpha")
{
    fs::path inPath = fs::path(inputBmp).make_preferred();
    fs::path outPath = fs::path(outputDds).make_preferred();
    std::string command = "\"" + g_Config.nvcompress + "\" " + format + " \"" + inPath.string() + "\" \"" + outPath.string() + "\"";
#ifdef _WIN32
    command = "\"" + command + "\"";
#endif
    int ret = std::system(command.c_str());
    if (ret != 0)
    {
        std::cerr << "Warning: nvcompress returned code " << ret << " for " << inputBmp << "\n";
    }
}

//=============================================
// @brief
//
//=============================================
void RunCompiler(const std::string& modelName)
{
#ifdef _WIN32
    fs::path binPath = fs::path(g_Config.target) / "vbmcompiler.exe";
    if (!fs::exists(binPath))
        binPath = "vbmcompiler.exe";
#else
    fs::path binPath = fs::path(g_Config.target) / "vbmcompiler";
    if (!fs::exists(binPath))
        binPath = "vbmcompiler";
#endif

    fs::path qcPath = fs::path(g_Config.target) / modelName / (modelName + ".qc");
    binPath.make_preferred();
    qcPath.make_preferred();
    std::string cmd = "\"" + binPath.string() + "\" -l \"" + qcPath.string() + "\"";
#ifdef _WIN32
    cmd = "\"" + cmd + "\"";
#endif
    int ret = std::system(cmd.c_str());
    if (ret != 0)
    {
        std::cerr << "Warning: vbmcompiler returned code " << ret << "\n";
    }
}

//=============================================
// @brief
//
//=============================================
std::string GetMaterialName(const cgltf_data* data, const cgltf_material* mat, const std::string& fallback)
{
    std::string name = fallback;
    if (mat)
    {
        if (mat->name && strlen(mat->name) > 0)
            name = mat->name;
        else if (data && data->materials)
            name = fallback + "_mat" + std::to_string(mat - data->materials);
    }
    std::replace(name.begin(), name.end(), ' ', '_');
    std::replace(name.begin(), name.end(), '.', '_');
    return name;
}

//=============================================
// @brief
//
//=============================================
void QuatToEuler(const float q[4], float euler[3])
{
    float x = q[0], y = q[1], z = q[2], w = q[3];

    float sinr_cosp = 2.0f * (w * x + y * z);
    float cosr_cosp = 1.0f - 2.0f * (x * x + y * y);
    euler[0] = std::atan2(sinr_cosp, cosr_cosp);

    float sinp = 2.0f * (w * y - z * x);
    if (std::abs(sinp) >= 1.0f)
        euler[1] = std::copysign(3.14159265358979323846f / 2.0f, sinp);
    else
        euler[1] = std::asin(sinp);

    float siny_cosp = 2.0f * (w * z + x * y);
    float cosy_cosp = 1.0f - 2.0f * (y * y + z * z);
    euler[2] = std::atan2(siny_cosp, cosy_cosp);
}

//=============================================
// @brief
//
//=============================================
std::vector<const cgltf_node*> BuildBoneHierarchy(cgltf_data* data, std::map<const cgltf_node*, int>& nodeToBone)
{
    nodeToBone.clear();
    std::vector<const cgltf_node*> bones;
    if (!data)
    {
        return bones;
    }

    if (data->skins_count > 0 && data->skins[0].joints_count > 0)
    {
        const cgltf_skin& skin = data->skins[0];
        std::set<const cgltf_node*> jointSet;
        for (cgltf_size i = 0; i < skin.joints_count; ++i)
        {
            jointSet.insert(skin.joints[i]);
        }

        std::vector<const cgltf_node*> sortedJoints;
        auto addJointRecursive = [&](auto& self, const cgltf_node* node) -> void 
            {
            if (!node)
                return;

            if (jointSet.count(node) && std::find(sortedJoints.begin(), sortedJoints.end(), node) == sortedJoints.end())
            {
                sortedJoints.push_back(node);
            }
            for (cgltf_size ci = 0; ci < node->children_count; ++ci)
            {
                self(self, node->children[ci]);
            }
        };

        for (cgltf_size ni = 0; ni < data->nodes_count; ++ni)
        {
            if (!data->nodes[ni].parent)
            {
                addJointRecursive(addJointRecursive, &data->nodes[ni]);
            }
        }

        for (cgltf_size i = 0; i < skin.joints_count; ++i)
        {
            if (std::find(sortedJoints.begin(), sortedJoints.end(), skin.joints[i]) == sortedJoints.end())
            {
                sortedJoints.push_back(skin.joints[i]);
            }
        }

        for (size_t i = 0; i < sortedJoints.size(); ++i)
        {
            nodeToBone[sortedJoints[i]] = static_cast<int>(i);
            bones.push_back(sortedJoints[i]);
        }
    }
    else
    {
        auto addNodeRecursive = [&](auto& self, const cgltf_node* node) -> void 
            {
            if (!node)
                return;

            if (nodeToBone.find(node) == nodeToBone.end())
            {
                nodeToBone[node] = static_cast<int>(bones.size());
                bones.push_back(node);
            }

            for (cgltf_size ci = 0; ci < node->children_count; ++ci)
            {
                self(self, node->children[ci]);
            }
        };

        for (cgltf_size ni = 0; ni < data->nodes_count; ++ni)
        {
            if (!data->nodes[ni].parent)
            {
                addNodeRecursive(addNodeRecursive, &data->nodes[ni]);
            }
        }

        for (cgltf_size ni = 0; ni < data->nodes_count; ++ni)
        {
            if (nodeToBone.find(&data->nodes[ni]) == nodeToBone.end())
            {
                addNodeRecursive(addNodeRecursive, &data->nodes[ni]);
            }
        }
    }

    return bones;
}

//=============================================
// @brief
//
//=============================================
void WriteSMDNodes(std::ofstream& out, const std::vector<const cgltf_node*>& bones, const std::map<const cgltf_node*, int>& nodeToBone, const std::string& fallback)
{
    out << "version 1\nnodes\n";
    if (bones.empty())
    {
        out << "  0 \"root\" -1\n";
    }
    else
    {
        std::set<std::string> usedNames;
        for (size_t i = 0; i < bones.size(); ++i)
        {
            const cgltf_node* node = bones[i];
            std::string name = node->name && strlen(node->name) > 0 ? node->name : (fallback + "_bone" + std::to_string(i));
            std::replace(name.begin(), name.end(), ' ', '_');

            if (usedNames.count(name))
            {
                name += "_" + std::to_string(i);
            }
            usedNames.insert(name);

            int parentIdx = -1;
            const cgltf_node* parentNode = node->parent;
            while (parentNode)
            {
                auto it = nodeToBone.find(parentNode);
                if (it != nodeToBone.end())
                {
                    parentIdx = it->second;
                    break;
                }
                parentNode = parentNode->parent;
            }

            out << "  " << i << " \"" << name << "\" " << parentIdx << "\n";
        }
    }
    out << "end\n";
}

//=============================================
// @brief
//
//=============================================
std::vector<AnimationSequenceInfo> ExportAnimations(const std::string& workDir, const std::string& assetName, cgltf_data* data, const std::vector<const cgltf_node*>& bones, const std::map<const cgltf_node*, int>& nodeToBone)
{
    std::vector<AnimationSequenceInfo> seqList;
    if (!data)
    {
        return seqList;
    }

    std::vector<BoneTransform> baseTransforms(bones.size());
    for (size_t b = 0; b < bones.size(); ++b)
    {
        const cgltf_node* node = bones[b];
        if (node->has_translation)
        {
            baseTransforms[b].pos[0] = node->translation[0];
            baseTransforms[b].pos[1] = node->translation[1];
            baseTransforms[b].pos[2] = node->translation[2];
        }
        if (node->has_rotation)
        {
            QuatToEuler(node->rotation, baseTransforms[b].rot);
        }
    }

    if (data->animations_count == 0)
    {
        std::string idleSmdName = assetName + "_idle.smd";
        std::string outPath = workDir + "/" + idleSmdName;
        std::ofstream out(outPath);
        if (out.is_open())
        {
            out << std::fixed << std::setprecision(6);
            WriteSMDNodes(out, bones, nodeToBone, assetName);
            out << "skeleton\ntime 0\n";
            if (bones.empty())
            {
                out << "  0 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000\n";
            }
            else
            {
                for (size_t b = 0; b < bones.size(); ++b)
                {
                    out << "  " << b << " "
                        << baseTransforms[b].pos[0] << " " << baseTransforms[b].pos[1] << " " << baseTransforms[b].pos[2] << " "
                        << baseTransforms[b].rot[0] << " " << baseTransforms[b].rot[1] << " " << baseTransforms[b].rot[2] << "\n";
                }
            }
            out << "end\n";
            seqList.push_back({ "idle1", idleSmdName, 1.0f, 1 });
        }
        return seqList;
    }

    for (cgltf_size ai = 0; ai < data->animations_count; ++ai)
    {
        const cgltf_animation& anim = data->animations[ai];
        std::string rawAnimName = anim.name && strlen(anim.name) > 0 ? anim.name : ("anim_" + std::to_string(ai));
        std::replace(rawAnimName.begin(), rawAnimName.end(), ' ', '_');

        float maxTime = 0.0f;
        for (cgltf_size ci = 0; ci < anim.channels_count; ++ci)
        {
            const cgltf_animation_sampler* sampler = anim.channels[ci].sampler;
            if (sampler && sampler->input && sampler->input->count > 0)
            {
                float lastVal = 0.0f;
                cgltf_accessor_read_float(sampler->input, sampler->input->count - 1, &lastVal, 1);
                if (lastVal > maxTime)
                {
                    maxTime = lastVal;
                }
            }
        }

        float fps = 30.0f;
        int frameCount = static_cast<int>(std::ceil(maxTime * fps)) + 1;
        if (frameCount < 1)
        {
            frameCount = 1;
        }

        std::string animSmdName = assetName + "_" + rawAnimName + ".smd";
        std::string outPath = workDir + "/" + animSmdName;
        std::ofstream out(outPath);
        if (!out.is_open())
        {
            continue;
        }

        out << std::fixed << std::setprecision(6);
        WriteSMDNodes(out, bones, nodeToBone, assetName);
        out << "skeleton\n";

        for (int f = 0; f < frameCount; ++f)
        {
            float t = (frameCount > 1) ? (static_cast<float>(f) / fps) : 0.0f;
            out << "time " << f << "\n";

            std::vector<BoneTransform> frameTransforms = baseTransforms;

            for (cgltf_size ci = 0; ci < anim.channels_count; ++ci)
            {
                const cgltf_animation_channel& channel = anim.channels[ci];
                const cgltf_node* targetNode = channel.target_node;
                auto it = nodeToBone.find(targetNode);
                if (it == nodeToBone.end())
                {
                    continue;
                }

                int boneIdx = it->second;
                const cgltf_animation_sampler* sampler = channel.sampler;
                if (!sampler || !sampler->input || !sampler->output || sampler->input->count == 0)
                {
                    continue;
                }

                cgltf_size keyCount = sampler->input->count;
                cgltf_size prevKey = 0;
                cgltf_size nextKey = 0;
                float prevT = 0.0f;
                float nextT = 0.0f;

                cgltf_accessor_read_float(sampler->input, 0, &prevT, 1);
                if (t <= prevT)
                {
                    prevKey = nextKey = 0;
                    nextT = prevT;
                }
                else
                {
                    cgltf_accessor_read_float(sampler->input, keyCount - 1, &nextT, 1);
                    if (t >= nextT)
                    {
                        prevKey = nextKey = keyCount - 1;
                        prevT = nextT;
                    }
                    else
                    {
                        for (cgltf_size k = 0; k < keyCount - 1; ++k)
                        {
                            float kt0 = 0.0f, kt1 = 0.0f;
                            cgltf_accessor_read_float(sampler->input, k, &kt0, 1);
                            cgltf_accessor_read_float(sampler->input, k + 1, &kt1, 1);
                            if (t >= kt0 && t <= kt1)
                            {
                                prevKey = k;
                                nextKey = k + 1;
                                prevT = kt0;
                                nextT = kt1;
                                break;
                            }
                        }
                    }
                }

                float factor = (nextT > prevT) ? ((t - prevT) / (nextT - prevT)) : 0.0f;
                if (sampler->interpolation == cgltf_interpolation_type_step)
                {
                    factor = 0.0f;
                }

                if (channel.target_path == cgltf_animation_path_type_translation)
                {
                    float p0[3]{ 0 }, p1[3]{ 0 };
                    cgltf_accessor_read_float(sampler->output, prevKey, p0, 3);
                    cgltf_accessor_read_float(sampler->output, nextKey, p1, 3);
                    frameTransforms[boneIdx].pos[0] = p0[0] + (p1[0] - p0[0]) * factor;
                    frameTransforms[boneIdx].pos[1] = p0[1] + (p1[1] - p0[1]) * factor;
                    frameTransforms[boneIdx].pos[2] = p0[2] + (p1[2] - p0[2]) * factor;
                }
                else if (channel.target_path == cgltf_animation_path_type_rotation)
                {
                    float q0[4]{ 0, 0, 0, 1 }, q1[4]{ 0, 0, 0, 1 };
                    cgltf_accessor_read_float(sampler->output, prevKey, q0, 4);
                    cgltf_accessor_read_float(sampler->output, nextKey, q1, 4);

                    float dot = q0[0] * q1[0] + q0[1] * q1[1] + q0[2] * q1[2] + q0[3] * q1[3];
                    if (dot < 0.0f)
                    {
                        q1[0] = -q1[0]; q1[1] = -q1[1]; q1[2] = -q1[2]; q1[3] = -q1[3];
                        dot = -dot;
                    }

                    float qOut[4];
                    if (dot > 0.9995f)
                    {
                        for (int c = 0; c < 4; ++c)
                            qOut[c] = q0[c] + factor * (q1[c] - q0[c]);
                    }
                    else
                    {
                        float theta0 = std::acos(std::clamp(dot, -1.0f, 1.0f));
                        float theta = theta0 * factor;
                        float sinTheta = std::sin(theta);
                        float sinTheta0 = std::sin(theta0);
                        float s0 = std::cos(theta) - dot * sinTheta / sinTheta0;
                        float s1 = sinTheta / sinTheta0;
                        for (int c = 0; c < 4; ++c)
                            qOut[c] = (s0 * q0[c]) + (s1 * q1[c]);
                    }

                    float qlen = std::sqrt(qOut[0] * qOut[0] + qOut[1] * qOut[1] + qOut[2] * qOut[2] + qOut[3] * qOut[3]);
                    if (qlen > 0.00001f)
                    {
                        qOut[0] /= qlen; qOut[1] /= qlen; qOut[2] /= qlen; qOut[3] /= qlen;
                    }

                    QuatToEuler(qOut, frameTransforms[boneIdx].rot);
                }
            }

            if (bones.empty())
            {
                out << "  0 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000\n";
            }
            else
            {
                for (size_t b = 0; b < bones.size(); ++b)
                {
                    out << "  " << b << " "
                        << frameTransforms[b].pos[0] << " " << frameTransforms[b].pos[1] << " " << frameTransforms[b].pos[2] << " "
                        << frameTransforms[b].rot[0] << " " << frameTransforms[b].rot[1] << " " << frameTransforms[b].rot[2] << "\n";
                }
            }
        }

        out << "end\n";
        seqList.push_back({ rawAnimName, animSmdName, fps, frameCount });
    }

    return seqList;
}

//=============================================
// @brief
//
//=============================================
void WriteSMD(const std::string& smd_filename, cgltf_data* data, const std::string& default_mat_name, const std::vector<const cgltf_node*>& bones, const std::map<const cgltf_node*, int>& nodeToBone)
{
    std::ofstream out(smd_filename);
    if (!out.is_open())
    {
        return;
    }

    out << std::fixed << std::setprecision(6);
    WriteSMDNodes(out, bones, nodeToBone, default_mat_name);

    out << "skeleton\ntime 0\n";
    if (bones.empty())
    {
        out << "  0 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000\n";
    }
    else
    {
        for (size_t b = 0; b < bones.size(); ++b)
        {
            const cgltf_node* node = bones[b];
            float p[3]{ 0.0f, 0.0f, 0.0f };
            float r[3]{ 0.0f, 0.0f, 0.0f };
            if (node->has_translation)
            {
                p[0] = node->translation[0];
                p[1] = node->translation[1];
                p[2] = node->translation[2];
            }
            if (node->has_rotation)
            {
                QuatToEuler(node->rotation, r);
            }
            out << "  " << b << " " << p[0] << " " << p[1] << " " << p[2] << " " << r[0] << " " << r[1] << " " << r[2] << "\n";
        }
    }
    out << "end\ntriangles\n";

    for (cgltf_size ni = 0; ni < data->nodes_count; ++ni)
    {
        const cgltf_node& node = data->nodes[ni];
        if (!node.mesh)
        {
            continue;
        }

        bool isSkinned = (node.skin && node.skin->joints_count > 0);
        float matrix[16];
        cgltf_node_transform_world(&node, matrix);

        int fallbackBoneIdx = 0;
        auto itNode = nodeToBone.find(&node);
        if (itNode != nodeToBone.end())
        {
            fallbackBoneIdx = itNode->second;
        }

        const cgltf_mesh& mesh = *node.mesh;
        for (cgltf_size pi = 0; pi < mesh.primitives_count; ++pi)
        {
            const cgltf_primitive& prim = mesh.primitives[pi];
            const cgltf_accessor* pos = nullptr, * norm = nullptr, * uv = nullptr;
            const cgltf_accessor* jointsAcc = nullptr, * weightsAcc = nullptr;

            for (cgltf_size ai = 0; ai < prim.attributes_count; ++ai)
            {
                const cgltf_attribute& attr = prim.attributes[ai];
                if (strcmp(attr.name, "POSITION") == 0)
                {
                    pos = attr.data;
                }
                else if (strcmp(attr.name, "NORMAL") == 0)
                {
                    norm = attr.data;
                }
                else if (strcmp(attr.name, "TEXCOORD_0") == 0)
                {
                    uv = attr.data;
                }
                else if (strcmp(attr.name, "JOINTS_0") == 0)
                {
                    jointsAcc = attr.data;
                }
                else if (strcmp(attr.name, "WEIGHTS_0") == 0)
                {
                    weightsAcc = attr.data;
                }
            }

            if (!pos || !prim.indices)
            {
                continue;
            }

            std::string texName = GetMaterialName(data, prim.material, default_mat_name) + ".bmp";
            for (cgltf_size t = 0; t < prim.indices->count; t += 3)
            {
                out << texName << "\n";
                for (int i = 0; i < 3; ++i)
                {
                    cgltf_uint idx = cgltf_accessor_read_index(prim.indices, t + i);
                    float p[3] = { 0 }, n[3] = { 0, 0, 1 }, u[2] = { 0, 0 };
                    cgltf_accessor_read_float(pos, idx, p, 3);
                    if (norm)
                    {
                        cgltf_accessor_read_float(norm, idx, n, 3);
                    }
                    if (uv)
                    {
                        cgltf_accessor_read_float(uv, idx, u, 2);
                    }

                    float tp[3], tn[3];
                    if (!isSkinned)
                    {
                        tp[0] = matrix[0] * p[0] + matrix[4] * p[1] + matrix[8] * p[2] + matrix[12];
                        tp[1] = matrix[1] * p[0] + matrix[5] * p[1] + matrix[9] * p[2] + matrix[13];
                        tp[2] = matrix[2] * p[0] + matrix[6] * p[1] + matrix[10] * p[2] + matrix[14];

                        tn[0] = matrix[0] * n[0] + matrix[4] * n[1] + matrix[8] * n[2];
                        tn[1] = matrix[1] * n[0] + matrix[5] * n[1] + matrix[9] * n[2];
                        tn[2] = matrix[2] * n[0] + matrix[6] * n[1] + matrix[10] * n[2];
                    }
                    else
                    {
                        tp[0] = p[0]; tp[1] = p[1]; tp[2] = p[2];
                        tn[0] = n[0]; tn[1] = n[1]; tn[2] = n[2];
                    }

                    float len = std::sqrt(tn[0] * tn[0] + tn[1] * tn[1] + tn[2] * tn[2]);
                    if (len > 0.00001f)
                    {
                        tn[0] /= len; tn[1] /= len; tn[2] /= len;
                    }

                    std::vector<std::pair<int, float>> boneWeights;
                    if (isSkinned && jointsAcc && weightsAcc)
                    {
                        cgltf_uint jIndices[4] = { 0 };
                        float wValues[4] = { 0.0f };
                        cgltf_accessor_read_uint(jointsAcc, idx, jIndices, 4);
                        cgltf_accessor_read_float(weightsAcc, idx, wValues, 4);

                        float totalWeight = 0.0f;
                        for (int k = 0; k < 4; ++k)
                        {
                            if (wValues[k] > 0.0001f)
                            {
                                if (jIndices[k] < node.skin->joints_count)
                                {
                                    const cgltf_node* jointNode = node.skin->joints[jIndices[k]];
                                    auto itJoint = nodeToBone.find(jointNode);
                                    if (itJoint != nodeToBone.end())
                                    {
                                        boneWeights.push_back({ itJoint->second, wValues[k] });
                                        totalWeight += wValues[k];
                                    }
                                }
                            }
                        }

                        if (totalWeight > 0.0001f)
                        {
                            for (auto& bw : boneWeights)
                            {
                                bw.second /= totalWeight;
                            }
                        }
                    }

                    if (boneWeights.empty())
                    {
                        boneWeights.push_back({ fallbackBoneIdx, 1.0f });
                    }

                    out << "  " << boneWeights[0].first << " "
                        << tp[0] << " " << tp[1] << " " << tp[2] << " "
                        << tn[0] << " " << tn[1] << " " << tn[2] << " "
                        << u[0] << " " << 1.0f - u[1] << " "
                        << boneWeights.size();

                    for (const auto& bw : boneWeights)
                    {
                        out << " " << bw.first << " " << bw.second;
                    }
                    out << "\n";
                }
            }
        }
    }
    out << "end\n";
}

//=============================================
// @brief
//
//=============================================
void WriteQC(const std::string& qc_filename, const std::string& model_name, const std::vector<AnimationSequenceInfo>& sequences)
{
    std::ofstream out(qc_filename);
    if (!out.is_open())
    {
        return;
    }

    out << "$modelname \"" << model_name << ".mdl\"\n"
        << "$cd \".\"\n"
        << "$cdtexture \".\"\n"
        << "$scale 50\n"
        << "$bodygroup \"studio\"\n"
        << "{\n"
        << "    studio \"" << model_name << "\" collision \"" << model_name << "\"\n"
        << "}\n";

    out << "$sequence \"idle1\" \"" << model_name << ".smd\" fps 1\n";

    for (size_t i = 0; i < sequences.size(); ++i)
    {
        out << "$sequence \"" << sequences[i].name << "\" \"" << sequences[i].smdName << "\" fps " << sequences[i].fps << "\n";
    }
}

//=============================================
// @brief
//
//=============================================
void WritePMF(const std::string& pmf_filename, const std::string& asset_name, const std::string& mat_name, ExportMode mode, bool isAlphaTest = false)
{
    std::ofstream out(pmf_filename);
    if (!out.is_open())
    {
        return;
    }

    std::string prefix = (mode == ExportMode::Model) ? ("models/" + asset_name + "/") : "world/";

    out << "$texture\n{\n";
    if (isAlphaTest)
    {
        out << "    $alphatest\n";
    }
    out << "    $texture diffuse " << prefix << mat_name << "_diff.dds\n";
    out << "    $texture normal " << prefix << mat_name << "_normal.dds\n";
    out << "    $texture mrao " << prefix << mat_name << "_mrao.dds\n";
    out << "    $cubemaps\n}\n";
}

//=============================================
// @brief
//
//=============================================
void ProcessFile(const fs::path& glbPath, ExportMode mode)
{
    std::string assetName = glbPath.stem().string();
    std::string workDir = "temp_output/" + assetName;
    fs::create_directories(workDir);

    cgltf_options options = {};
    cgltf_data* data = nullptr;
    if (cgltf_parse_file(&options, glbPath.string().c_str(), &data) != cgltf_result_success)
    {
        return;
    }
    cgltf_load_buffers(&options, data, glbPath.string().c_str());

    if (data->materials_count > 0)
    {
        for (cgltf_size mi = 0; mi < data->materials_count; ++mi)
        {
            const cgltf_material& mat = data->materials[mi];
            std::string matName = GetMaterialName(data, &mat, assetName);
            std::string bmpPath = workDir + "/" + matName + ".bmp";

            bool isAlphaTest = (mat.alpha_mode == cgltf_alpha_mode_mask);
            if (mat.pbr_metallic_roughness.base_color_texture.texture)
            {
                if (SaveGLTFImageAsBMP(mat.pbr_metallic_roughness.base_color_texture.texture->image, bmpPath, false, 0, 0, isAlphaTest ? 32 : 24))
                {
                    ConvertBMPToDDS(bmpPath, workDir + "/" + matName + "_diff.dds", isAlphaTest ? "-bc3" : "-bc1");
                    SaveGLTFImageAsBMP(mat.pbr_metallic_roughness.base_color_texture.texture->image, bmpPath, true, 512, 512);
                }
            }
            else
            {
                uint8_t r = static_cast<uint8_t>(std::clamp(mat.pbr_metallic_roughness.base_color_factor[0] * 255.0f, 0.0f, 255.0f));
                uint8_t g = static_cast<uint8_t>(std::clamp(mat.pbr_metallic_roughness.base_color_factor[1] * 255.0f, 0.0f, 255.0f));
                uint8_t b = static_cast<uint8_t>(std::clamp(mat.pbr_metallic_roughness.base_color_factor[2] * 255.0f, 0.0f, 255.0f));
                uint8_t a = static_cast<uint8_t>(std::clamp(mat.pbr_metallic_roughness.base_color_factor[3] * 255.0f, 0.0f, 255.0f));

                std::vector<uint8_t> solid(16 * 16 * 4);
                for (size_t i = 0; i < 16 * 16; ++i)
                {
                    solid[i * 4 + 0] = r;
                    solid[i * 4 + 1] = g;
                    solid[i * 4 + 2] = b;
                    solid[i * 4 + 3] = a;
                }

                WriteBMP(bmpPath, 16, 16, solid.data(), isAlphaTest ? 32 : 24);
                ConvertBMPToDDS(bmpPath, workDir + "/" + matName + "_diff.dds", isAlphaTest ? "-bc3" : "-bc1");
                WriteBMP8Indexed(bmpPath, 16, 16, solid.data());
            }

            if (mat.normal_texture.texture)
            {
                if (SaveGLTFImageAsBMP(mat.normal_texture.texture->image, workDir + "/temp_n.bmp", false))
                {
                    ConvertBMPToDDS(workDir + "/temp_n.bmp", workDir + "/" + matName + "_normal.dds");
                }
            }

            if (SaveMRAOTexture(mat, workDir + "/temp_s.bmp"))
            {
                ConvertBMPToDDS(workDir + "/temp_s.bmp", workDir + "/" + matName + "_mrao.dds", "-bc3");
            }

            WritePMF(workDir + "/" + matName + ".pmf", assetName, matName, mode, isAlphaTest);
        }
    }
    else
    {
        std::string bmpPath = workDir + "/" + assetName + ".bmp";
        std::vector<uint8_t> dummy(16 * 16 * 4, 255);
        WriteBMP8Indexed(bmpPath, 16, 16, dummy.data());
        WritePMF(workDir + "/" + assetName + ".pmf", assetName, assetName, mode);
    }

    fs::path targetPath = fs::path(g_Config.target) / assetName;
    fs::create_directories(targetPath);

    for (const auto& entry : fs::directory_iterator(workDir))
    {
        fs::copy_file(entry.path(), targetPath / entry.path().filename(), fs::copy_options::overwrite_existing);
    }

    if (mode == ExportMode::Model)
    {
        std::map<const cgltf_node*, int> nodeToBone;
        std::vector<const cgltf_node*> bones = BuildBoneHierarchy(data, nodeToBone);

        WriteSMD(workDir + "/" + assetName + ".smd", data, assetName, bones, nodeToBone);
        std::vector<AnimationSequenceInfo> sequences = ExportAnimations(workDir, assetName, data, bones, nodeToBone);
        WriteQC(workDir + "/" + assetName + ".qc", assetName, sequences);

        fs::copy_file(workDir + "/" + assetName + ".smd", targetPath / (assetName + ".smd"), fs::copy_options::overwrite_existing);
        fs::copy_file(workDir + "/" + assetName + ".qc", targetPath / (assetName + ".qc"), fs::copy_options::overwrite_existing);

        for (const auto& seq : sequences)
        {
            if (fs::exists(workDir + "/" + seq.smdName))
            {
                fs::copy_file(workDir + "/" + seq.smdName, targetPath / seq.smdName, fs::copy_options::overwrite_existing);
            }
        }

        cgltf_free(data);
        RunCompiler(assetName);

        fs::path gameModelsRoot = fs::path(g_Config.game) / "models";
        fs::path gameTexturesRoot = fs::path(g_Config.game) / "textures" / "models" / assetName;

        fs::create_directories(gameModelsRoot);
        fs::create_directories(gameTexturesRoot);

        if (fs::exists(targetPath / (assetName + ".mdl")))
            fs::copy_file(targetPath / (assetName + ".mdl"), gameModelsRoot / (assetName + ".mdl"), fs::copy_options::overwrite_existing);

        if (fs::exists(targetPath / (assetName + ".mcd")))
            fs::copy_file(targetPath / (assetName + ".mcd"), gameModelsRoot / (assetName + ".mcd"), fs::copy_options::overwrite_existing);

        if (fs::exists(targetPath / (assetName + ".vbm")))
            fs::copy_file(targetPath / (assetName + ".vbm"), gameModelsRoot / (assetName + ".vbm"), fs::copy_options::overwrite_existing);

        for (const auto& entry : fs::directory_iterator(targetPath))
        {
            if (entry.path().extension() == ".dds" || entry.path().extension() == ".pmf")
            {
                fs::copy_file(entry.path(), gameTexturesRoot / entry.path().filename(), fs::copy_options::overwrite_existing);
            }
        }
    }
    else
    {
        cgltf_free(data);

        fs::path gameWorldTexturesRoot = fs::path(g_Config.game) / "textures" / "world";
        fs::create_directories(gameWorldTexturesRoot);

        for (const auto& entry : fs::directory_iterator(targetPath))
        {
            if (entry.path().extension() == ".dds" || entry.path().extension() == ".pmf")
            {
                fs::copy_file(entry.path(), gameWorldTexturesRoot / entry.path().filename(), fs::copy_options::overwrite_existing);
            }
        }
    }

    std::cout << "Finished and Copied: " << assetName << (mode == ExportMode::Model ? " (Model)" : " (Texture)") << "\n";
}

//=============================================
// @brief
//
//=============================================
void PrintUsage()
{
    std::cout << "Usage:\n" 
            << "  mdlimporter -model <file.glb>\n" 
            << "  mdlimporter -texture <file.glb>\n" 
            << "  mdlimporter -batch <folder> -model\n"
            << "  mdlimporter -batch <folder> -texture\n";
}

//=============================================
// @brief
//
//=============================================
int main(int argc, char* argv[])
{
    LoadConfig();

    if (argc < 3)
    {
        PrintUsage();
        return 0;
    }

    std::string arg1 = argv[1];

    if (arg1 == "-batch")
    {
        if (argc < 4)
        {
            PrintUsage();
            return 1;
        }

        fs::path searchDir = argv[2];
        std::string modeArg = argv[3];

        ExportMode mode = ExportMode::Model;
        if (modeArg == "-texture")
        {
            mode = ExportMode::Texture;
        }
        else if (modeArg != "-model")
        {
            PrintUsage();
            return 1;
        }

        if (!fs::exists(searchDir))
        {
            std::cout << "Error: Folder " << searchDir << " does not exist.\n";
            return 1;
        }

        for (const auto& entry : fs::directory_iterator(searchDir))
        {
            if (entry.path().extension() == ".glb")
            {
                ProcessFile(entry.path(), mode);
            }
        }
    }
    else if (arg1 == "-model" || arg1 == "-texture")
    {
        ExportMode mode = (arg1 == "-texture") ? ExportMode::Texture : ExportMode::Model;
        fs::path p = argv[2];
        if (fs::exists(p) && p.extension() == ".glb")
        {
            ProcessFile(p, mode);
        }
        else
        {
            std::cout << "Error: File " << p << " does not exist or is not a .glb file.\n";
        }
    }
    else
    {
        PrintUsage();
    }

    return 0;
}