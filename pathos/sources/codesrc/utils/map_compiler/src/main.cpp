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
#include <cstdio>
#include <iostream>
#include <string>
#include <chrono>
#include "datatypes.h"
#include "mapparser.h"
#include "brush.h"
#include "bsp.h"
#include "lightmap.h"
#include "rad.h"
#include "vis.h"

int main(int argc, char* argv[])
{
    std::cout << "=========================================\n";
    std::cout << "Matrix Engine 2 Map Compiler\n";
    std::cout << "=========================================\n\n";

    auto startTime = std::chrono::high_resolution_clock::now();

    std::string basePath = "";
    std::string gamedir = "";
    std::string daystage = "";
    for (int i = 1; i < argc; i++)
    {
        if ((strcmp(argv[i], "-gamedir") == 0 || strcmp(argv[i], "-moddir") == 0) && i + 1 < argc)
        {
            gamedir = argv[++i];
        }
        else if (strcmp(argv[i], "-daystage") == 0 && i + 1 < argc)
        {
            daystage = argv[++i];
        }
        else if (argv[i][0] != '-')
        {
            basePath = argv[i];
        }
    }

    if (basePath.empty())
    {
        std::cout << "Usage: " << argv[0] << " [-gamedir <path>] <path_to_map_file>\n";
        return 1;
    }

    size_t dotPos = basePath.find_last_of('.');
    if (dotPos != std::string::npos)
    {
        basePath = basePath.substr(0, dotPos);
    }

    std::string mapFile = basePath + ".map";
    std::string dispFile = basePath + ".mapdisp";
    std::string outputFile = basePath + ".bsp";
    std::string aldFile = basePath + ".ald";

    map_data_t mapData;
    if (!ParseMapFile(mapFile.c_str(), mapData))
    {
        std::cerr << "Error: Failed to open " << mapFile << "\n";
        return 1;
    }

    map_disp_data_t dispData;
    ParseMapDisp(dispFile.c_str(), dispData);

    std::vector<lightmap_face_t> faceLightmaps;
    if (!ProcessMapGeometry(mapData, dispData, faceLightmaps))
    {
        std::cerr << "Error: Failed to process map geometry.\n";
        return 1;
    }

    CRadPipeline rad;
    if (rad.InitializeEmbree())
    {
        rad.BuildSceneGeometry(mapData, dispData, gamedir.c_str());
        rad.ParseLights(mapData, daystage);

        std::cout << "Computing PVS Visibility...\n";
        CalculatePVS(&rad);

        std::cout << "Baking lightmaps...\n";
        rad.BakeLightmaps(faceLightmaps, gamedir.c_str());
        rad.BakeVertexLights(mapData, gamedir.c_str());
        g_BSP.SetEntities(SerializeEntities(mapData));
        rad.BuildLightGrid(32);
        rad.Shutdown();
    }

    if (!daystage.empty())
    {
        aldlumptype_t lumpType = ALD_LUMP_NIGHTDATA_BUMP;
        if (daystage == "daylightreturn")
            lumpType = ALD_LUMP_DAYLIGHT_RETURN_DATA_BUMP;

        std::cout << "Exporting ALD stage '" << daystage << "' to '" << aldFile << "'...\n";
        if (!g_BSP.ExportALD(aldFile.c_str(), lumpType))
        {
            std::cerr << "Error: Failed to export ALD file.\n";
            return 1;
        }
    }
    else
    {
        std::cout << "Writing BSP file: " << outputFile << "\n";
        if (!g_BSP.ExportFile(outputFile.c_str()))
        {
            std::cerr << "Error: Failed to write output file.\n";
            return 1;
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = endTime - startTime;
    std::cout << "\nCompilation completed successfully. Took " << elapsed.count() << " seconds.\n";
    return 0;
}