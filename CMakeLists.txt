cmake_minimum_required (VERSION 3.6)
project (ProgParallel)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${PROJECT_SOURCE_DIR}/cmake)

find_package(Threads REQUIRED)
find_package(benchmark REQUIRED)

INCLUDE(FindPkgConfig)
pkg_search_module(SDL2 REQUIRED sdl2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "-W -Wall -march=native")
set(CMAKE_CXX_RELEASE_FLAGS "${CMAKE_CXX_RELEASE_FLAGS} -O3 -ltbb")

include_directories(lib/include)

add_library(renderer src/render.hpp src/render.cpp)
add_executable(view src/view.cpp)
target_include_directories(view PRIVATE ${SDL2_INCLUDE_DIRS})

target_link_libraries(view renderer ${SDL2_LIBRARIES} Threads::Threads tbb)


link_directories(/usr/lib)

add_executable(bench src/bench.cpp)
target_link_libraries(bench renderer benchmark::benchmark tbb)
