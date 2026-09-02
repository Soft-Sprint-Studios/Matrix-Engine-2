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

bool SaveGLTFImageAsBMP(const cgltf_image* image, const std::string& out_filename, bool is8bppIndexed, int targetW = 0, int targetH = 0)
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
        return WriteBMP(out_filename, finalW, finalH, finalPixels.data(), 24);
    }
}

bool SaveMRAOTexture(const cgltf_material& mat, const std::string& out_filename)
{
    const cgltf_image* mr_image = mat.pbr_metallic_roughness.metallic_roughness_texture.texture ? mat.pbr_metallic_roughness.metallic_roughness_texture.texture->image : nullptr;
    const cgltf_image* occ_image = mat.occlusion_texture.texture ? mat.occlusion_texture.texture->image : nullptr;
    const cgltf_image* em_image = mat.emissive_texture.texture ? mat.emissive_texture.texture->image : nullptr;

    if (!mr_image && !occ_image && !em_image)
    {
        return false;
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

void WriteSMD(const std::string& smd_filename, cgltf_data* data, const std::string& texture_name)
{
    std::ofstream out(smd_filename);
    if (!out.is_open())
    {
        return;
    }

    out << std::fixed << std::setprecision(6);
    out << "version 1\nnodes\n  0 \"root\" -1\nend\nskeleton\ntime 0\n  0 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000\nend\ntriangles\n";

    for (cgltf_size mi = 0; mi < data->meshes_count; ++mi)
    {
        const cgltf_mesh& mesh = data->meshes[mi];
        for (cgltf_size pi = 0; pi < mesh.primitives_count; ++pi)
        {
            const cgltf_primitive& prim = mesh.primitives[pi];
            const cgltf_accessor* pos = nullptr, * norm = nullptr, * uv = nullptr;

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
            }

            if (!pos || !prim.indices)
            {
                continue;
            }

            for (cgltf_size t = 0; t < prim.indices->count; t += 3)
            {
                out << texture_name << "\n";
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
                    out << "  0 " << p[0] << " " << p[1] << " " << p[2] << " " << n[0] << " " << n[1] << " " << n[2] << " " << u[0] << " " << 1.0f - u[1] << " 1 0 1.000\n";
                }
            }
        }
    }
    out << "end\n";
}

void WriteQC(const std::string& qc_filename, const std::string& model_name)
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
        << "}\n"
        << "$sequence \"idle1\" \"" << model_name << "\" fps 1\n";
}

void WritePMF(const std::string& pmf_filename, const std::string& asset_name, ExportMode mode)
{
    std::ofstream out(pmf_filename);
    if (!out.is_open())
    {
        return;
    }

    std::string prefix = (mode == ExportMode::Model) ? ("models/" + asset_name + "/") : "world/";

    out << "$texture\n{\n";
    out << "    $texture diffuse " << prefix << asset_name << "_diff.dds\n";
    out << "    $texture normal " << prefix << asset_name << "_normal.dds\n";
    out << "    $texture mrao " << prefix << asset_name << "_mrao.dds\n";
    out << "    $cubemaps\n}\n";
}

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

    std::string bmpPath = workDir + "/" + assetName + ".bmp";

    if (data->materials_count > 0)
    {
        const cgltf_material& mat = data->materials[0];
        if (mat.pbr_metallic_roughness.base_color_texture.texture)
        {
            if (SaveGLTFImageAsBMP(mat.pbr_metallic_roughness.base_color_texture.texture->image, bmpPath, false, 0, 0))
            {
                ConvertBMPToDDS(bmpPath, workDir + "/" + assetName + "_diff.dds");
                SaveGLTFImageAsBMP(mat.pbr_metallic_roughness.base_color_texture.texture->image, bmpPath, true, 512, 512);
            }
        }
        if (mat.normal_texture.texture)
        {
            if (SaveGLTFImageAsBMP(mat.normal_texture.texture->image, workDir + "/temp_n.bmp", false))
            {
                ConvertBMPToDDS(workDir + "/temp_n.bmp", workDir + "/" + assetName + "_normal.dds");
            }
        }

        if (SaveMRAOTexture(mat, workDir + "/temp_s.bmp"))
        {
            ConvertBMPToDDS(workDir + "/temp_s.bmp", workDir + "/" + assetName + "_mrao.dds", "-bc3");
        }
    }

    WritePMF(workDir + "/" + assetName + ".pmf", assetName, mode);

    fs::path targetPath = fs::path(g_Config.target) / assetName;
    fs::create_directories(targetPath);

    for (const auto& entry : fs::directory_iterator(workDir))
    {
        fs::copy_file(entry.path(), targetPath / entry.path().filename(), fs::copy_options::overwrite_existing);
    }

    if (mode == ExportMode::Model)
    {
        WriteSMD(workDir + "/" + assetName + ".smd", data, assetName + ".bmp");
        WriteQC(workDir + "/" + assetName + ".qc", assetName);

        fs::copy_file(workDir + "/" + assetName + ".smd", targetPath / (assetName + ".smd"), fs::copy_options::overwrite_existing);
        fs::copy_file(workDir + "/" + assetName + ".qc", targetPath / (assetName + ".qc"), fs::copy_options::overwrite_existing);

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

        fs::copy_file(targetPath / (assetName + ".pmf"), gameTexturesRoot / (assetName + ".pmf"), fs::copy_options::overwrite_existing);

        for (const auto& entry : fs::directory_iterator(targetPath))
        {
            if (entry.path().extension() == ".dds")
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

        fs::copy_file(targetPath / (assetName + ".pmf"), gameWorldTexturesRoot / (assetName + ".pmf"), fs::copy_options::overwrite_existing);

        for (const auto& entry : fs::directory_iterator(targetPath))
        {
            if (entry.path().extension() == ".dds")
            {
                fs::copy_file(entry.path(), gameWorldTexturesRoot / entry.path().filename(), fs::copy_options::overwrite_existing);
            }
        }
    }

    std::cout << "Finished and Copied: " << assetName << (mode == ExportMode::Model ? " (Model)" : " (Texture)") << "\n";
}

void PrintUsage()
{
    std::cout << "Usage:\n"
              << "  mdlimporter -model <file.glb>\n"
              << "  mdlimporter -texture <file.glb>\n"
              << "  mdlimporter -batch <folder> -model\n"
              << "  mdlimporter -batch <folder> -texture\n";
}

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