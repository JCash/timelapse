#! /usr/bin/env bash

OPT="-O0 -g"

ASAN="-fsanitize=address -fno-omit-frame-pointer -fsanitize-address-use-after-scope -fsanitize=undefined"
ASAN_LDFLAGS="-fsanitize=address "

clang++ $OPT $ASAN $ASAN_LDFLAGS test/version_test.cpp src/version.cpp

# INCLUDES="-Iexternal -Iexternal/imgui -I/usr/local/Cellar/freetype/2.10.1/include/freetype2"
# FRAMEWORKS="-framework Foundation -framework AppKit -framework Metal -framework MetalKit -framework QuartzCore"
# clang++ $OPT $INCLUDES -Lbuild -L/usr/local/lib -lfreetype -lhelp_darwin $FRAMEWORKS test/imguifonts.cpp external/imgui/imgui_demo.cpp