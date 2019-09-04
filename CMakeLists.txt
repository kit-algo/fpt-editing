cmake_minimum_required(VERSION 3.13)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")

project(F-free_Edge_Editing)

set(CMAKE_CXX_STANDARD 17)

find_package(GUROBI REQUIRED)

include_directories(SYSTEM ${GUROBI_INCLUDE_DIRS})

add_subdirectory(external/tlx)

add_executable(edit_gurobi src/main_gurobi.cpp src/Graph/Graph.cpp)
target_link_libraries(edit_gurobi ${GUROBI_LIBRARIES} tlx)

add_executable(json2csv json2csv.cpp)
target_link_libraries(json2csv tlx)

add_executable(test_finder src/test_finder.cpp src/Graph/Graph.cpp)
