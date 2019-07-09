#! /usr/bin/env bash

OPT="-O0 -g"

ASAN="-fsanitize=address -fno-omit-frame-pointer -fsanitize-address-use-after-scope -fsanitize=undefined"
ASAN_LDFLAGS="-fsanitize=address "

clang++ $OPT $ASAN $ASAN_LDFLAGS test/version_test.cpp src/version.cpp