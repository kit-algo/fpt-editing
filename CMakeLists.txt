cmake_minimum_required(VERSION 3.13)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

project(F-free_Edge_Editing)

set(CMAKE_CXX_STANDARD 17)

find_package(GUROBI REQUIRED)

include_directories(SYSTEM ${GUROBI_INCLUDE_DIRS})

add_subdirectory(external/tlx)

add_executable(edit_gurobi src/main_gurobi.cpp src/Graph/Graph.cpp)
target_link_libraries(edit_gurobi ${GUROBI_LIBRARIES} tlx)
