cmake_minimum_required(VERSION 3.20)
project(VerletPhysics)

set(CMAKE_CXX_STANDARD 23)

add_executable(
        VerletPhysics
        src/Main.cpp
        src/gl.c)
target_include_directories(VerletPhysics PRIVATE src)
target_compile_options(VerletPhysics PRIVATE -D_CRT_SECURE_NO_WARNINGS)
target_link_libraries(VerletPhysics PRIVATE OpenGL32)
