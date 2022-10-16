cmake_minimum_required(VERSION 3.23)
project(FlatBuffer)

set(CMAKE_CXX_STANDARD 17)

find_package(Boost 1.78 REQUIRED)
find_package(fmt REQUIRED)

add_library(flatbuffers STATIC flatbuffers/AST.h
        flatbuffers/AST.cpp
        flatbuffers/Parser.cpp
        flatbuffers/Parser.h
        flatbuffers/Compiler.cpp
        flatbuffers/Compiler.h flatbuffers/Error.cpp flatbuffers/Error.h flatbuffers/CodeGenerator.cpp)
target_link_libraries(flatbuffers PUBLIC Boost::boost fmt::fmt)

add_executable(fb_compiler main.cpp)
target_link_libraries(fb_compiler flatbuffers)
