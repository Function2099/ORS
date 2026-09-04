cmake_minimum_required(VERSION 3.20)

project(ORS
    VERSION 0.0.0
    DESCRIPTION "Open Recording Software"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Phase 0: project layout only.
# TODO: find_package(Qt6), add_executable, platform sources, tests.
#
# Preferred local build dir (gitignored): ignore/build
#   cmake -B ignore/build -S .
#   cmake --build ignore/build
