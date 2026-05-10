cmake_minimum_required(VERSION 3.20)

project(PmxMod)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "Vcpkg target triplet")
set(VCPKG_INCLUDE_DIR "C:/vcpkg/installed/${VCPKG_TARGET_TRIPLET}/include")
list(PREPEND CMAKE_PREFIX_PATH "C:/vcpkg/installed/${VCPKG_TARGET_TRIPLET}")
list(PREPEND CMAKE_MODULE_PATH "C:/vcpkg/installed/${VCPKG_TARGET_TRIPLET}/share/stb")

find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Bullet CONFIG REQUIRED)
find_package(Stb MODULE REQUIRED)
find_path(MINIAUDIO_INCLUDE_DIR miniaudio.h HINTS "${VCPKG_INCLUDE_DIR}" REQUIRED)

add_executable(PmxMod
        main.cpp
        src/Util.h
        src/IkSolver.cpp src/IkSolver.h
        src/Model.cpp src/Model.h
        src/Node.cpp src/Node.h
        src/Physics.cpp src/Physics.h
        src/Reader.cpp src/Reader.h
        src/Animation.cpp src/Animation.h
        src/Sound.cpp src/Sound.h
        viewer/Viewer.cpp viewer/Viewer.h
        viewer/GLFWViewer.cpp viewer/GLFWViewer.h
        viewer/DX11Viewer.cpp viewer/DX11Viewer.h
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
target_include_directories(PmxMod PRIVATE
        ${Stb_INCLUDE_DIR}
        ${MINIAUDIO_INCLUDE_DIR}
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

if (WIN32)
    add_custom_command(TARGET PmxMod POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "C:/vcpkg/installed/${VCPKG_TARGET_TRIPLET}/bin/glfw3.dll"
            "$<TARGET_FILE_DIR:PmxMod>"
            VERBATIM
    )
endif ()
