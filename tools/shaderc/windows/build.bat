@echo off
setlocal

for /F "delims=" %%T in ('vswhere -latest -format value -property catalog_productLineVersion') do set VS_VER=%%T
for /F "delims=" %%T in ('vswhere -latest -format value -property installationPath') do set VS_PATH=%%T

if not defined VS_VER goto :no_vs_found
if not defined VS_PATH goto :no_vs_found
goto :vs_found

:no_vs_found
echo ERROR: Can't find any Visual Studio installation.
goto :eof

:vs_found
echo Detected VS version and path: %VS_VER%, %VS_PATH%

:: TODO : Not sure how far back this path format is stable.
set MSBUILD_EXE="%VS_PATH%\MSBuild\Current\Bin\MSBuild.exe"

if not exist %MSBUILD_EXE% (
    echo ERROR: Can't locate MSBuild.exe at expected location: %MSBUILD_EXE%
    goto :eof
)

:: https://developercommunity.visualstudio.com/t/windows-sdk-81-gone-from-vs2019/517851
if "%VS_VER%" == "2019" (
    set SDK_VER=10.0
) else (
    set SDK_VER=8.1
)
echo Setting wanted Windows SDK version as: %SDK_VER%

set BGFX_DIR=..\..\..\third_party\bgfx
pushd %BGFX_DIR%

..\bx\tools\bin\windows\genie --with-tools --with-windows=%SDK_VER% vs%VS_VER%

%MSBUILD_EXE% .build/projects/vs%VS_VER%/shaderc.vcxproj /p:configuration=release /p:platform=x64

popd

copy /Y "%BGFX_DIR%\.build\win64_vs%VS_VER%\bin\shadercRelease.exe" ".\shaderc.exe"
rd /S /Q "%BGFX_DIR%\.build"