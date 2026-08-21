@echo off
if not exist build_windows_x64 (
    mkdir build_windows_x64
)

cd build_windows_x64
cmake -G "Visual Studio 17 2022" ..