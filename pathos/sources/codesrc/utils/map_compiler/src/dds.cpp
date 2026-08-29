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
#include "dds.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

#define DDS_MAGIC 0x20534444

#define DDPF_FOURCC 0x00000004
#define DDPF_RGB    0x00000040
#define DDPF_ALPHAPIXELS 0x00000001

#define FOURCC_DXT1 0x31545844
#define FOURCC_DXT3 0x33545844
#define FOURCC_DXT5 0x35545844

struct dds_pixel_format_t
{
    Uint32 size;
    Uint32 flags;
    Uint32 fourCC;
    Uint32 rgbBitCount;
    Uint32 rBitMask;
    Uint32 gBitMask;
    Uint32 bBitMask;
    Uint32 aBitMask;
};

struct dds_header_t
{
    Uint32 magic;
    Uint32 size;
    Uint32 flags;
    Uint32 height;
    Uint32 width;
    Uint32 pitchOrLinearSize;
    Uint32 depth;
    Uint32 mipMapCount;
    Uint32 reserved1[11];
    dds_pixel_format_t ddspf;
    Uint32 caps;
    Uint32 caps2;
    Uint32 caps3;
    Uint32 caps4;
    Uint32 reserved2;
};

static void DecodeDXT1Block(const byte* block, byte* outRgba, Int32 width, Int32 x, Int32 y)
{
    Uint16 c0 = block[0] | (block[1] << 8);
    Uint16 c1 = block[2] | (block[3] << 8);

    byte colors[4][4];

    colors[0][0] = ((c0 >> 11) & 0x1F) * 255 / 31;
    colors[0][1] = ((c0 >> 5) & 0x3F) * 255 / 63;
    colors[0][2] = (c0 & 0x1F) * 255 / 31;
    colors[0][3] = 255;

    colors[1][0] = ((c1 >> 11) & 0x1F) * 255 / 31;
    colors[1][1] = ((c1 >> 5) & 0x3F) * 255 / 63;
    colors[1][2] = (c1 & 0x1F) * 255 / 31;
    colors[1][3] = 255;

    if (c0 > c1)
    {
        colors[2][0] = (2 * colors[0][0] + colors[1][0]) / 3;
        colors[2][1] = (2 * colors[0][1] + colors[1][1]) / 3;
        colors[2][2] = (2 * colors[0][2] + colors[1][2]) / 3;
        colors[2][3] = 255;

        colors[3][0] = (colors[0][0] + 2 * colors[1][0]) / 3;
        colors[3][1] = (colors[0][1] + 2 * colors[1][1]) / 3;
        colors[3][2] = (colors[0][2] + 2 * colors[1][2]) / 3;
        colors[3][3] = 255;
    }
    else
    {
        colors[2][0] = (colors[0][0] + colors[1][0]) / 2;
        colors[2][1] = (colors[0][1] + colors[1][1]) / 2;
        colors[2][2] = (colors[0][2] + colors[1][2]) / 2;
        colors[2][3] = 255;

        colors[3][0] = 0;
        colors[3][1] = 0;
        colors[3][2] = 0;
        colors[3][3] = 0;
    }

    Uint32 code = block[4] | (block[5] << 8) | (block[6] << 16) | (block[7] << 24);

    for (Int32 by = 0; by < 4; by++)
    {
        for (Int32 bx = 0; bx < 4; bx++)
        {
            Int32 idx = (code >> (2 * (by * 4 + bx))) & 0x03;
            size_t outOffset = ((y + by) * width + (x + bx)) * 4;
            memcpy(&outRgba[outOffset], colors[idx], 4);
        }
    }
}

static void DecodeDXT5Block(const byte* block, byte* outRgba, Int32 width, Int32 x, Int32 y)
{
    byte a0 = block[0];
    byte a1 = block[1];
    byte alphas[8];
    alphas[0] = a0;
    alphas[1] = a1;

    if (a0 > a1)
    {
        for (Int32 i = 1; i <= 6; i++)
        {
            alphas[i + 1] = ((7 - i) * a0 + i * a1) / 7;
        }
    }
    else
    {
        for (Int32 i = 1; i <= 4; i++)
        {
            alphas[i + 1] = ((5 - i) * a0 + i * a1) / 5;
        }
        alphas[6] = 0;
        alphas[7] = 255;
    }

    Uint64 alphaBits = 0;
    for (Int32 i = 0; i < 6; i++)
    {
        alphaBits |= ((Uint64)block[2 + i]) << (i * 8);
    }

    DecodeDXT1Block(block + 8, outRgba, width, x, y);

    for (Int32 by = 0; by < 4; by++)
    {
        for (Int32 bx = 0; bx < 4; bx++)
        {
            Int32 bitShift = 3 * (by * 4 + bx);
            Int32 alphaIdx = (Int32)((alphaBits >> bitShift) & 0x07);
            size_t outOffset = ((y + by) * width + (x + bx)) * 4 + 3;
            outRgba[outOffset] = alphas[alphaIdx];
        }
    }
}

static void CalculateReflectivity(dds_image_t& img)
{
    double totalR = 0.0;
    double totalG = 0.0;
    double totalB = 0.0;
    size_t pixelCount = (size_t)img.width * img.height;

    for (size_t i = 0; i < pixelCount; i++)
    {
        totalR += pow((Float)img.rgba[i * 4 + 0] / 255.0f, 2.2f);
        totalG += pow((Float)img.rgba[i * 4 + 1] / 255.0f, 2.2f);
        totalB += pow((Float)img.rgba[i * 4 + 2] / 255.0f, 2.2f);
    }

    img.reflectivity[0] = (Float)(totalR / (double)pixelCount);
    img.reflectivity[1] = (Float)(totalG / (double)pixelCount);
    img.reflectivity[2] = (Float)(totalB / (double)pixelCount);
}

bool LoadDDSFromMemory(const byte* fileBuffer, size_t fileSize, dds_image_t& outImage)
{
    if (fileSize < sizeof(dds_header_t))
    {
        return false;
    }

    const dds_header_t* hdr = reinterpret_cast<const dds_header_t*>(fileBuffer);
    if (hdr->magic != DDS_MAGIC)
    {
        return false;
    }

    outImage.width = (Int32)hdr->width;
    outImage.height = (Int32)hdr->height;
    outImage.channels = 4;
    outImage.rgba.resize(outImage.width * outImage.height * 4);

    const byte* pixelData = fileBuffer + 128;

    if (hdr->ddspf.flags & DDPF_FOURCC)
    {
        if (hdr->ddspf.fourCC == FOURCC_DXT1)
        {
            Int32 bx = (outImage.width + 3) / 4;
            Int32 by = (outImage.height + 3) / 4;
            const byte* src = pixelData;

            for (Int32 y = 0; y < by; y++)
            {
                for (Int32 x = 0; x < bx; x++)
                {
                    DecodeDXT1Block(src, outImage.rgba.data(), outImage.width, x * 4, y * 4);
                    src += 8;
                }
            }
        }
        else if (hdr->ddspf.fourCC == FOURCC_DXT5)
        {
            Int32 bx = (outImage.width + 3) / 4;
            Int32 by = (outImage.height + 3) / 4;
            const byte* src = pixelData;

            for (Int32 y = 0; y < by; y++)
            {
                for (Int32 x = 0; x < bx; x++)
                {
                    DecodeDXT5Block(src, outImage.rgba.data(), outImage.width, x * 4, y * 4);
                    src += 16;
                }
            }
        }
        else
        {
            return false;
        }
    }
    else if (hdr->ddspf.flags & DDPF_RGB)
    {
        size_t bpp = hdr->ddspf.rgbBitCount / 8;
        for (Int32 i = 0; i < outImage.width * outImage.height; i++)
        {
            outImage.rgba[i * 4 + 0] = pixelData[i * bpp + 2];
            outImage.rgba[i * 4 + 1] = pixelData[i * bpp + 1];
            outImage.rgba[i * 4 + 2] = pixelData[i * bpp + 0];
            outImage.rgba[i * 4 + 3] = (bpp == 4) ? pixelData[i * bpp + 3] : 255;
        }
    }
    else
    {
        return false;
    }

    CalculateReflectivity(outImage);
    return true;
}

bool LoadDDSFromFile(const Char* filename, dds_image_t& outImage)
{
    FILE* f = fopen(filename, "rb");
    if (!f)
    {
        return false;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0)
    {
        fclose(f);
        return false;
    }

    std::vector<byte> buf(sz);
    fread(buf.data(), 1, sz, f);
    fclose(f);

    return LoadDDSFromMemory(buf.data(), buf.size(), outImage);
}