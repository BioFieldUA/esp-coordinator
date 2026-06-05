# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(PROJECT_VER "1.0.2.6")

string(REPLACE "." ";" VER_LIST ${PROJECT_VER})
list(GET VER_LIST 0 MAJ)
list(GET VER_LIST 1 MIN)
list(GET VER_LIST 2 PAT)
list(GET VER_LIST 3 BLD)
math(EXPR VER_HEX "(${MAJ} << 24) | (${MIN} << 16) | (${PAT} << 8) | ${BLD}")
add_compile_definitions(PROJECT_VER_UINT32=${VER_HEX})
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp-coordinator)
