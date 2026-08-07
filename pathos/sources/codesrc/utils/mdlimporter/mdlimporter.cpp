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
#define _CRT_SECURE_NO_WARNINGS
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include <windows.h>
#include <commdlg.h>
#include <gdiplus.h>
#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <map>
#include <iomanip>

#pragma comment(lib, "gdiplus.lib")

namespace fs = std::filesystem;

struct AppConfig
{
    std::string nvcompress = "C:\\Program Files\\NVIDIA Corporation\\NVIDIA Texture Tools\\nvcompress.exe";
    std::string target = "D:\\VS2022PROJECTS\\Matrix-Engine-2\\pathos\\sources\\progs";
    std::string game = "D:\\VS2022PROJECTS\\Matrix-Engine-2\\pathos";
};

AppConfig g_Config;
ULONG_PTR gdiToken;

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

void InitGDIPlus()
{
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&gdiToken, &gdiplusStartupInput, nullptr);
}

void ShutdownGDIPlus()
{
    Gdiplus::GdiplusShutdown(gdiToken);
}

bool SaveGLTFImageAsBMP(const cgltf_image* image, const std::string& out_filename, Gdiplus::PixelFormat format, int targetW = 0, int targetH = 0)
{
    if (!image || !image->buffer_view || !image->buffer_view->buffer)
    {
        return false;
    }

    const uint8_t* data = (const uint8_t*)image->buffer_view->buffer->data + image->buffer_view->offset;
    size_t size = image->buffer_view->size;

    IStream* stream = nullptr;
    if (CreateStreamOnHGlobal(nullptr, TRUE, &stream) != S_OK)
    {
        return false;
    }

    ULONG written;
    stream->Write(data, (ULONG)size, &written);
    stream->Seek({ 0 }, STREAM_SEEK_SET, nullptr);

    Gdiplus::Bitmap* source = Gdiplus::Bitmap::FromStream(stream);
    if (!source || source->GetLastStatus() != Gdiplus::Ok)
    {
        stream->Release();
        return false;
    }

    int finalW = (targetW > 0) ? targetW : source->GetWidth();
    int finalH = (targetH > 0) ? targetH : source->GetHeight();

    Gdiplus::Bitmap target(finalW, finalH, format);
    Gdiplus::Graphics g(&target);
    g.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    g.DrawImage(source, 0, 0, finalW, finalH);

    CLSID clsid;
    UINT num = 0, size_needed = 0;
    Gdiplus::GetImageEncodersSize(&num, &size_needed);
    std::vector<BYTE> encoders(size_needed);
    Gdiplus::ImageCodecInfo* codecInfo = (Gdiplus::ImageCodecInfo*)encoders.data();
    Gdiplus::GetImageEncoders(num, size_needed, codecInfo);

    for (UINT i = 0; i < num; ++i)
    {
        if (wcscmp(codecInfo[i].MimeType, L"image/bmp") == 0)
        {
            clsid = codecInfo[i].Clsid;
            break;
        }
    }

    std::wstring wfilename(out_filename.begin(), out_filename.end());
    target.Save(wfilename.c_str(), &clsid, nullptr);

    delete source;
    stream->Release();
    return true;
}

bool SaveMRAOTexture(const cgltf_material& mat, const std::string& out_filename)
{
    const cgltf_image* mr_image = mat.pbr_metallic_roughness.metallic_roughness_texture.texture ? mat.pbr_metallic_roughness.metallic_roughness_texture.texture->image : nullptr;
    const cgltf_image* occ_image = mat.occlusion_texture.texture ? mat.occlusion_texture.texture->image : nullptr;

    if (!mr_image && !occ_image)
    {
        return false;
    }

    auto LoadBitmapFromImage = [](const cgltf_image* img) -> Gdiplus::Bitmap*
        {
            if (!img || !img->buffer_view || !img->buffer_view->buffer)
            {
                return nullptr;
            }
            const uint8_t* data = (const uint8_t*)img->buffer_view->buffer->data + img->buffer_view->offset;
            size_t size = img->buffer_view->size;
            IStream* stream = nullptr;
            if (CreateStreamOnHGlobal(nullptr, TRUE, &stream) != S_OK)
            {
                return nullptr;
            }
            ULONG written;
            stream->Write(data, (ULONG)size, &written);
            stream->Seek({ 0 }, STREAM_SEEK_SET, nullptr);
            Gdiplus::Bitmap* bmp = Gdiplus::Bitmap::FromStream(stream);
            stream->Release();
            if (!bmp || bmp->GetLastStatus() != Gdiplus::Ok)
            {
                delete bmp;
                return nullptr;
            }
            return bmp;
        };

    Gdiplus::Bitmap* mr_source = LoadBitmapFromImage(mr_image);
    Gdiplus::Bitmap* occ_source = LoadBitmapFromImage(occ_image);

    if (!mr_source && !occ_source)
    {
        return false;
    }

    UINT width = mr_source ? mr_source->GetWidth() : occ_source->GetWidth();
    UINT height = mr_source ? mr_source->GetHeight() : occ_source->GetHeight();

    Gdiplus::Bitmap target(width, height, PixelFormat24bppRGB);

    for (UINT y = 0; y < height; ++y)
    {
        for (UINT x = 0; x < width; ++x)
        {
            BYTE ao = 255;
            BYTE roughness = 128;
            BYTE metallic = 0;

            if (mr_source)
            {
                UINT mr_x = (x * mr_source->GetWidth()) / width;
                UINT mr_y = (y * mr_source->GetHeight()) / height;
                Gdiplus::Color c_mr;
                mr_source->GetPixel(mr_x, mr_y, &c_mr);

                roughness = c_mr.GetG();
                metallic = c_mr.GetB();
                ao = c_mr.GetR();
            }

            if (occ_source)
            {
                UINT occ_x = (x * occ_source->GetWidth()) / width;
                UINT occ_y = (y * occ_source->GetHeight()) / height;
                Gdiplus::Color c_occ;
                occ_source->GetPixel(occ_x, occ_y, &c_occ);

                ao = c_occ.GetR();
            }

            target.SetPixel(x, y, Gdiplus::Color(ao, roughness, metallic));
        }
    }

    CLSID clsid;
    UINT num = 0, size_needed = 0;
    Gdiplus::GetImageEncodersSize(&num, &size_needed);
    std::vector<BYTE> encoders(size_needed);
    Gdiplus::ImageCodecInfo* codecInfo = (Gdiplus::ImageCodecInfo*)encoders.data();
    Gdiplus::GetImageEncoders(num, size_needed, codecInfo);

    for (UINT i = 0; i < num; ++i)
    {
        if (wcscmp(codecInfo[i].MimeType, L"image/bmp") == 0)
        {
            clsid = codecInfo[i].Clsid;
            break;
        }
    }

    std::wstring wfilename(out_filename.begin(), out_filename.end());
    target.Save(wfilename.c_str(), &clsid, nullptr);

    delete mr_source;
    delete occ_source;
    return true;
}

void ConvertBMPToDDS(const std::string& inputBmp, const std::string& outputDds)
{
    std::string command = "\"" + g_Config.nvcompress + "\" -bc1 -alpha \"" + inputBmp + "\" \"" + outputDds + "\"";

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;

    if (CreateProcessA(NULL, const_cast<char*>(command.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi))
    {
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

void RunCompiler(const std::string& modelName)
{
    fs::path binPath = fs::path(g_Config.target) / "vbmcompiler.exe";
    fs::path qcPath = fs::path(g_Config.target) / modelName / (modelName + ".qc");
    std::string cmd = "\"\"" + binPath.string() + "\" -l \"" + qcPath.string() + "\"\"";
    system(cmd.c_str());
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

void WritePMF(const std::string& pmf_filename, const std::string& model_name, bool hasLuminance = false)
{
    std::ofstream out(pmf_filename);
    if (!out.is_open())
    {
        return;
    }
    out << "$texture\n{\n";
    out << "    $texture diffuse models/" << model_name << "/" << model_name << "_diff.dds\n";
    out << "    $texture normal models/" << model_name << "/" << model_name << "_normal.dds\n";
    out << "    $texture specular models/" << model_name << "/" << model_name << "_specular.dds\n";
    if (hasLuminance)
    {
        out << "    $texture luminance models/" << model_name << "/" << model_name << "_lum.dds\n";
    }
    out << "    $cubemaps\n}\n";
}

void ProcessFile(const fs::path& glbPath)
{
    std::string modelName = glbPath.stem().string();
    std::string workDir = "temp_output/" + modelName;
    fs::create_directories(workDir);

    cgltf_options options = {};
    cgltf_data* data = nullptr;
    if (cgltf_parse_file(&options, glbPath.string().c_str(), &data) != cgltf_result_success)
    {
        return;
    }
    cgltf_load_buffers(&options, data, glbPath.string().c_str());

    std::string bmpPath = workDir + "/" + modelName + ".bmp";

    bool hasLuminance = false;

    if (data->materials_count > 0)
    {
        const cgltf_material& mat = data->materials[0];
        if (mat.pbr_metallic_roughness.base_color_texture.texture)
        {
            if (SaveGLTFImageAsBMP(mat.pbr_metallic_roughness.base_color_texture.texture->image, bmpPath, PixelFormat24bppRGB, 0, 0))
            {
                ConvertBMPToDDS(bmpPath, workDir + "/" + modelName + "_diff.dds");
                SaveGLTFImageAsBMP(mat.pbr_metallic_roughness.base_color_texture.texture->image, bmpPath, PixelFormat8bppIndexed, 512, 512);
            }
        }
        if (mat.normal_texture.texture)
        {
            if (SaveGLTFImageAsBMP(mat.normal_texture.texture->image, workDir + "/temp_n.bmp", PixelFormat24bppRGB))
            {
                ConvertBMPToDDS(workDir + "/temp_n.bmp", workDir + "/" + modelName + "_normal.dds");
            }
        }

        if (SaveMRAOTexture(mat, workDir + "/temp_s.bmp"))
        {
            ConvertBMPToDDS(workDir + "/temp_s.bmp", workDir + "/" + modelName + "_specular.dds");
        }

        if (mat.emissive_texture.texture)
        {
            if (SaveGLTFImageAsBMP(mat.emissive_texture.texture->image, workDir + "/temp_l.bmp", PixelFormat24bppRGB))
            {
                ConvertBMPToDDS(workDir + "/temp_l.bmp", workDir + "/" + modelName + "_lum.dds");
                hasLuminance = true;
            }
        }
    }

    WriteSMD(workDir + "/" + modelName + ".smd", data, modelName + ".bmp");
    WriteQC(workDir + "/" + modelName + ".qc", modelName);
    WritePMF(workDir + "/" + modelName + ".pmf", modelName, hasLuminance);

    fs::path targetPath = fs::path(g_Config.target) / modelName;
    fs::create_directories(targetPath);
    for (const auto& entry : fs::directory_iterator(workDir))
    {
        fs::copy_file(entry.path(), targetPath / entry.path().filename(), fs::copy_options::overwrite_existing);
    }

    cgltf_free(data);
    RunCompiler(modelName);

    fs::path gameModelsRoot = fs::path(g_Config.game) / "models";
    fs::path gameTexturesRoot = fs::path(g_Config.game) / "textures" / "models" / modelName;

    fs::create_directories(gameModelsRoot);
    fs::create_directories(gameTexturesRoot);

    fs::copy_file(targetPath / (modelName + ".mdl"), gameModelsRoot / (modelName + ".mdl"), fs::copy_options::overwrite_existing);
    fs::copy_file(targetPath / (modelName + ".mcd"), gameModelsRoot / (modelName + ".mcd"), fs::copy_options::overwrite_existing);
    fs::copy_file(targetPath / (modelName + ".vbm"), gameModelsRoot / (modelName + ".vbm"), fs::copy_options::overwrite_existing);
    fs::copy_file(targetPath / (modelName + ".pmf"), gameTexturesRoot / (modelName + ".pmf"), fs::copy_options::overwrite_existing);

    for (const auto& entry : fs::directory_iterator(targetPath))
    {
        if (entry.path().extension() == ".dds")
        {
            fs::copy_file(entry.path(), gameTexturesRoot / entry.path().filename(), fs::copy_options::overwrite_existing);
        }
    }

    std::cout << "Finished and Copied: " << modelName << "\n";
}

int main(int argc, char* argv[])
{
    LoadConfig();
    InitGDIPlus();

    if (argc < 2)
    {
        std::cout << "Usage:\n  importer <file.glb>\n  importer -batch <folder>\n";
        ShutdownGDIPlus();
        return 0;
    }

    std::string arg1 = argv[1];
    if (arg1 == "-batch")
    {
        if (argc < 3)
        {
            return 1;
        }

        fs::path searchDir = argv[2];
        if (!fs::exists(searchDir))
        {
            return 1;
        }

        for (const auto& entry : fs::directory_iterator(searchDir))
        {
            if (entry.path().extension() == ".glb")
            {
                ProcessFile(entry.path());
            }
        }
    }
    else
    {
        fs::path p = arg1;
        if (fs::exists(p) && p.extension() == ".glb")
        {
            ProcessFile(p);
        }
    }

    ShutdownGDIPlus();
    return 0;
}