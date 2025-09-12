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
        main.cpp
        base/File.cpp base/File.h
        base/Path.cpp base/Path.h
        base/UnicodeUtil.cpp base/UnicodeUtil.h
        src/MMDCamera.cpp src/MMDCamera.h
        src/MMDIkSolver.cpp src/MMDIkSolver.h
        src/MMDMaterial.cpp src/MMDMaterial.h
        src/MMDModel.cpp src/MMDModel.h
        src/MMDMorph.cpp src/MMDMorph.h
        src/MMDNode.cpp src/MMDNode.h
        src/MMDPhysics.cpp src/MMDPhysics.h
        src/PMXFile.cpp src/PMXFile.h
        src/VMDAnimation.cpp src/VMDAnimation.h
        src/VMDCameraAnimation.cpp src/VMDCameraAnimation.h
        src/VMDFile.cpp src/VMDFile.h
        src/stb_image.h
        AppContext.cpp AppContext.h
        MMDShader.cpp MMDShader.h
        Model.cpp Model.h
        miniaudio.h
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

set(RESOURCE_DIR ${CMAKE_SOURCE_DIR}/resource)
add_custom_command(TARGET PmxMod POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${RESOURCE_DIR} $<TARGET_FILE_DIR:PmxMod>/resource
)
