#!/bin/bash
BGFX_DIR=../../../third_party/bgfx
pushd $BGFX_DIR
make shaderc osx-x64-release
popd
cp -f "$BGFX_DIR/.build/osx-x64/bin/shadercRelease" "./shaderc"
rm -rf "$BGFX_DIR/.build"