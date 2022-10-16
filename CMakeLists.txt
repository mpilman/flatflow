cmake_minimum_required(VERSION 3.23)
project(FlatBuffer)

set(CMAKE_CXX_STANDARD 17)

find_package(Boost 1.78 REQUIRED COMPONENTS filesystem)
find_package(fmt REQUIRED)

add_library(flowflat STATIC flowflat.cpp include/flowflat/flowflat.h)
target_include_directories(flowflat PUBLIC include)

add_library(flatbuffers STATIC flatbuffers/AST.h
        flatbuffers/AST.cpp
        flatbuffers/Parser.cpp
        flatbuffers/Parser.h
        flatbuffers/Compiler.cpp
        flatbuffers/Compiler.h
        flatbuffers/Error.cpp
        flatbuffers/Error.h
        flatbuffers/CodeGenerator.cpp
        flatbuffers/Config.cpp
        flatbuffers/Config.h)
target_link_libraries(flatbuffers PUBLIC Boost::filesystem fmt::fmt flowflat)

add_executable(flowflatc main.cpp)
target_link_libraries(flowflatc flatbuffers)
