@echo off

powershell -Command "Invoke-WebRequest -Uri https://www.nuget.org/api/v2/package/Microsoft.Direct3D.D3D12/1.614.1 -OutFile agility.zip"
powershell -Command "& {Expand-Archive agility.zip External/DirectXAgilitySDK}"

xcopy External\DirectXAgilitySDK\build\native\bin\x64\* bin\Debug\D3D12\
xcopy External\DirectXAgilitySDK\build\native\bin\x64\* bin\Release\D3D12\


powershell -Command "Invoke-WebRequest -Uri https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.7.2212/dxc_2022_12_16.zip -OutFile dxc.zip"
powershell -Command "& {Expand-Archive dxc.zip External/DirectXShaderCompiler}"

xcopy External\DirectXShaderCompiler\bin\x64\* bin\Debug\
xcopy External\DirectXShaderCompiler\bin\x64\* bin\Release\

xcopy External\DirectXShaderCompiler\lib\x64\* bin\Debug\
xcopy External\DirectXShaderCompiler\lib\x64\* bin\Release\
