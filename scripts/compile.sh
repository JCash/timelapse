#! /usr/bin/env bash

PLATFORM=$1
if [ "$PLATFORM" == "" ]; then
    PLATFORM=`uname`
fi
PLATFORM=`echo $PLATFORM | tr '[:upper:]' '[:lower:]'`


set -e

OPT="-O3 -g"
OPT="-O0 -g"
CXX="clang++"
CCFLAGS="$OPT -DIMGUI_DISABLE_INCLUDE_IMCONFIG_H -I./src -I./external"
CXXFLAGS="-c"
LDFLAGS=""
BUILDDIR="./build"
TARGET="timelapse"

mkdir -p $BUILDDIR

if [ "$PLATFORM" == "darwin" ]; then
    #OPT="-O1 -g -fsanitize-address-use-after-scope -fsanitize=address -fno-omit-frame-pointer"
    ARGS="-framework Foundation $ARGS"
    APPLEFLAGS="-fobjc-arc -fmodules -x objective-c++"
    FRAMEWORKS="-framework Foundation -framework AppKit -framework Metal -framework MetalKit -framework QuartzCore"
elif [ "$PLATFORM" == "html5" ]; then
    CXX="em++"
    CXXFLAGS="-std=c++98 -Wno-c++11-compat"
    #OPT="-O0 -g"
    #LDFLAGS="--memory-init-file 0 -s WASM=1 --shell-file index.html"
    LDFLAGS="-s "BINARYEN_TRAP_MODE='clamp'" --shell-file shell.html -s TOTAL_MEMORY=134217728"
    TARGET="index.html"
fi

LIBRARYHELP=$BUILDDIR/libhelp_$PLATFORM.a
# Only comment out when the file is missing
if [ ! -e "$LIBRARYHELP" ]; then
    # ImGui and Sokol takes ~10s to compile which hinders fast iterations
    time $CXX $OPT $CXXFLAGS $CCFLAGS external/imgui/imgui_widgets.cpp -o $BUILDDIR/imgui_widgets.o
    time $CXX $OPT $CXXFLAGS $CCFLAGS external/imgui/imgui_draw.cpp -o $BUILDDIR/imgui_draw.o
    time $CXX $OPT $CXXFLAGS $CCFLAGS external/imgui/imgui.cpp -o $BUILDDIR/imgui.o
    time $CXX $OPT $CXXFLAGS $CCFLAGS $APPLEFLAGS external/sokol/sokol_imgui.cpp -o $BUILDDIR/sokol_imgui.o
    ar rcs $LIBRARYHELP $BUILDDIR/sokol_imgui.o $BUILDDIR/imgui.o $BUILDDIR/imgui_draw.o $BUILDDIR/imgui_widgets.o
else
    echo "$LIBRARYHELP already Exists!"
fi

$CXX $OPT $CXXFLAGS $CCFLAGS src/misc.cpp -o $BUILDDIR/misc.o
$CXX $OPT $CXXFLAGS $CCFLAGS src/viewer.cpp -o $BUILDDIR/viewer.o

$CXX $OPT $LDFLAGS $FRAMEWORKS -o $BUILDDIR/$TARGET -L$BUILDDIR -lhelp_$PLATFORM $BUILDDIR/viewer.o $BUILDDIR/misc.o
