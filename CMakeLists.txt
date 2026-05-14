cmake_minimum_required(VERSION 3.20)

project(PmxMod)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(VCPKG_ROOT "C:/vcpkg" CACHE PATH "Path to vcpkg root")
set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "Vcpkg target triplet")
set(VCPKG_INSTALLED_DIR "${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}")
set(VCPKG_INCLUDE_DIR "${VCPKG_INSTALLED_DIR}/include")
list(PREPEND CMAKE_PREFIX_PATH "${VCPKG_INSTALLED_DIR}")
list(PREPEND CMAKE_MODULE_PATH "${VCPKG_INSTALLED_DIR}/share/stb")

find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Bullet CONFIG REQUIRED)
find_package(Stb MODULE REQUIRED)
find_path(MINIAUDIO_INCLUDE_DIR miniaudio.h HINTS "${VCPKG_INCLUDE_DIR}" REQUIRED)

add_executable(PmxMod
        main.cpp
        src/Program/Program.h src/Program/Program.cpp
        src/Program/Controller.h src/Program/Controller.cpp
        src/Program/Sound.h src/Program/Sound.cpp
        
        src/Reader/PmxReader.h src/Reader/PmxReader.cpp
        src/Reader/VmdReader.h src/Reader/VmdReader.cpp
        src/Reader/ReaderHelper.h src/Reader/ReaderHelper.cpp

        src/Animation/Animation.h src/Animation/Animation.cpp
        src/Animation/CameraAnimation.h src/Animation/CameraAnimation.cpp
        src/Animation/AnimationHelper.h src/Animation/AnimationHelper.cpp
        src/Animation/Bezier.h src/Animation/Bezier.cpp
        
        src/Util.h src/Util.cpp
        src/IkSolver.h src/IkSolver.cpp
        src/Model.h src/Model.cpp
        src/Node.h src/Node.cpp
        src/Physics.h src/Physics.cpp

        src/Viewer/Viewer.h src/Viewer/Viewer.cpp
        src/Viewer/GlfwViewer.h src/Viewer/GlfwViewer.cpp
        src/Viewer/GlfwShaderHelper.h src/Viewer/GlfwShaderHelper.cpp
        src/Viewer/Dx11Viewer.h src/Viewer/Dx11Viewer.cpp
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
add_custom_command(TARGET PmxMod POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${VCPKG_INSTALLED_DIR}/bin/glfw3.dll"
        "$<TARGET_FILE_DIR:PmxMod>"
        VERBATIM
)
