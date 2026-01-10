cmake_minimum_required(VERSION 3.20)

project(PmxMod)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glfw3  REQUIRED)
find_package(glad   REQUIRED)
find_package(glm    REQUIRED)
find_package(Bullet CONFIG REQUIRED)

add_executable(PmxMod
        main.cpp
        external/miniaudio.h
        external/stb_image.h
        src/MMDUtil.cpp         src/MMDUtil.h
        src/MMDIkSolver.cpp     src/MMDIkSolver.h
        src/MMDModel.cpp        src/MMDModel.h
        src/MMDNode.cpp         src/MMDNode.h
        src/MMDPhysics.cpp      src/MMDPhysics.h
        src/MMDReader.cpp       src/MMDReader.h
        src/VMDAnimation.cpp    src/VMDAnimation.h
        viewer/Viewer.cpp       viewer/Viewer.h
        viewer/GLFWViewer.cpp   viewer/GLFWViewer.h
        viewer/DX11Viewer.cpp   viewer/DX11Viewer.h
)

target_link_libraries(PmxMod PUBLIC
        glfw
        glad::glad
        glm::glm

        d3d11
        dxgi
        d3dcompiler

        BulletDynamics
        BulletCollision
        LinearMath
)
target_compile_definitions(PmxMod PUBLIC)

set(RESOURCE_SRC_DIR "${CMAKE_SOURCE_DIR}/resource")
set(RESOURCE_OUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/resource")

add_custom_target(SyncResources
        COMMAND ${CMAKE_COMMAND} -E make_directory "${RESOURCE_OUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory  "${RESOURCE_SRC_DIR}" "${RESOURCE_OUT_DIR}"
        VERBATIM
)

add_dependencies(PmxMod SyncResources)