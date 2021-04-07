@echo off
setlocal

set MSBUILD_EXE=
for /D %%D in (%SYSTEMROOT%\Microsoft.NET\Framework\v4*) do set MSBUILD_EXE=%%D\MSBuild.exe

if not defined MSBUILD_EXE echo Error: Can't find MSBuild.exe. & goto :eof
if not exist "%MSBUILD_EXE%" echo Error: %MSBUILD_EXE%: not found. & goto :eof

set BGFX_DIR=..\..\..\third_party\bgfx
pushd %BGFX_DIR%

..\bx\tools\bin\windows\genie --with-tools vs2019

:: TODO : Fix this.
%MSBUILD_EXE% .build/projects/vs2019/shaderc.vcxproj /p:configuration=release /p:platform=x64

popd

copy /Y "%BGFX_DIR%\.build\win64_vs2019\bin\shadercRelease.exe" ".\shaderc.exe"
rd /S /Q "%BGFX_DIR%\.build"