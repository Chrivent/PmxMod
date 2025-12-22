cmake_minimum_required(VERSION 3.20)

set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

project(PmxMod)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glfw3  CONFIG REQUIRED)
find_package(glad   CONFIG REQUIRED)
find_package(glm    CONFIG REQUIRED)
find_package(Bullet CONFIG REQUIRED)

add_executable(PmxMod
        main.cpp
        AppContext.cpp AppContext.h
        MMDShader.cpp  MMDShader.h
        Model.cpp      Model.h
        external/miniaudio.h
        base/Util.h
        src/MMDIkSolver.cpp        src/MMDIkSolver.h
        src/MMDMaterial.cpp        src/MMDMaterial.h
        src/MMDModel.cpp           src/MMDModel.h
        src/MMDMorph.cpp           src/MMDMorph.h
        src/MMDNode.cpp            src/MMDNode.h
        src/MMDPhysics.cpp         src/MMDPhysics.h
        src/MMDReader.cpp          src/MMDReader.h
        src/VMDAnimation.cpp       src/VMDAnimation.h
        src/VMDCameraAnimation.cpp src/VMDCameraAnimation.h
        external/stb_image.h
)

target_link_libraries(PmxMod PUBLIC
        glfw
        glad::glad
        glm::glm
        BulletDynamics
        BulletCollision
        LinearMath
)
target_compile_definitions(PmxMod PUBLIC)

set(RESOURCE_DIR ${CMAKE_SOURCE_DIR}/resource)
add_custom_command(TARGET PmxMod POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${RESOURCE_DIR} $<TARGET_FILE_DIR:PmxMod>/resource
)
