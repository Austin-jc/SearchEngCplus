cmake_minimum_required(VERSION 3.19)
project(MSCI541)

set(CMAKE_CXX_STANDARD 20)


add_executable(ctp queryTextParser.cpp)
add_executable(booleanAND booleanAND.cpp)
add_executable( indexEngine IndexEngine.cpp)
add_executable(bm25 BM25.cpp)
add_executable(notGoogle notGoogle.cpp)
add_executable(getDoc  GetDoc.cpp)
include_directories(zlib-1.2.11)
include_directories(OleanderStemmingLibrary)
find_package(ZLIB REQUIRED)
target_link_libraries(indexEngine ZLIB::ZLIB)
target_link_libraries(getDoc ZLIB::ZLIB)
