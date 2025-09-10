cmake_minimum_required(VERSION 3.10)

set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

project(PmxMod)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glfw3 CONFIG REQUIRED)
find_package(glad  CONFIG REQUIRED)
find_package(glm   CONFIG REQUIRED)
find_package(Bullet CONFIG REQUIRED)

add_executable(PmxMod
    "Main.cpp"
    "File.cpp"
    "File.h"
    "Path.cpp"
    "Path.h"
    "UnicodeUtil.cpp"
    "UnicodeUtil.h"
    "MMDCamera.cpp"
    "MMDCamera.h"
    "MMDFileString.h"
    "MMDIkSolver.cpp"
    "MMDIkSolver.h"
    "MMDMaterial.cpp"
    "MMDMaterial.h"
    "MMDModel.cpp"
    "MMDModel.h"
    "MMDMorph.cpp"
    "MMDMorph.h"
    "MMDNode.cpp"
    "MMDNode.h"
    "MMDPhysics.cpp"
    "MMDPhysics.h"
    "PMXFile.cpp"
    "PMXFile.h"
    "VMDAnimation.cpp"
    "VMDAnimation.h"
    "VMDCameraAnimation.cpp"
    "VMDCameraAnimation.h"
    "VMDFile.cpp"
    "VMDFile.h"
    "stb_image.h"
        AppContext.cpp
        AppContext.h
)

target_link_libraries(PmxMod PRIVATE
    glfw
    glad::glad
    glm::glm
    BulletDynamics
    BulletCollision
    LinearMath
)

target_compile_definitions(PmxMod PRIVATE NOMINMAX)
