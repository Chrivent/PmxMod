cmake_minimum_required(VERSION 3.20)

set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

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
set(SHADER_DX11_SRC_DIR "${RESOURCE_SRC_DIR}/shader_DX11")
set(SHADER_DX11_OUT_DIR "${RESOURCE_OUT_DIR}/shader_DX11")

add_custom_target(SyncResources
        COMMAND ${CMAKE_COMMAND} -E make_directory "${RESOURCE_OUT_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy_directory  "${RESOURCE_SRC_DIR}" "${RESOURCE_OUT_DIR}"
        VERBATIM
)

find_program(FXC_EXE   NAMES fxc   REQUIRED)

function(add_fxc_shader OUT_VAR SRC PROFILE ENTRY OUT_BASENAME)
    set(OUT_FILE "${SHADER_DX11_OUT_DIR}/${OUT_BASENAME}.cso")
    add_custom_command(
            OUTPUT "${OUT_FILE}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_DX11_OUT_DIR}"
            COMMAND "${FXC_EXE}" /nologo /T "${PROFILE}" /E "${ENTRY}" /Fo "${OUT_FILE}" "${SRC}"
            DEPENDS "${SRC}"
            VERBATIM
    )
    set(${OUT_VAR} "${OUT_FILE}" PARENT_SCOPE)
endfunction()

add_fxc_shader(MMD_VS_CSO  "${SHADER_DX11_SRC_DIR}/mmd.fx"               vs_5_0 VSMain mmd_vs)
add_fxc_shader(MMD_PS_CSO  "${SHADER_DX11_SRC_DIR}/mmd.fx"               ps_5_0 PSMain mmd_ps)
add_fxc_shader(EDGE_VS_CSO "${SHADER_DX11_SRC_DIR}/mmd_edge.fx"          vs_5_0 VSMain mmd_edge_vs)
add_fxc_shader(EDGE_PS_CSO "${SHADER_DX11_SRC_DIR}/mmd_edge.fx"          ps_5_0 PSMain mmd_edge_ps)
add_fxc_shader(GS_VS_CSO   "${SHADER_DX11_SRC_DIR}/mmd_ground_shadow.fx" vs_5_0 VSMain mmd_gs_vs)
add_fxc_shader(GS_PS_CSO   "${SHADER_DX11_SRC_DIR}/mmd_ground_shadow.fx" ps_5_0 PSMain mmd_gs_ps)

add_custom_target(CompileDX11Shaders DEPENDS
        "${MMD_VS_CSO}" "${MMD_PS_CSO}"
        "${EDGE_VS_CSO}" "${EDGE_PS_CSO}"
        "${GS_VS_CSO}" "${GS_PS_CSO}"
)

add_dependencies(CompileDX11Shaders SyncResources)
add_dependencies(PmxMod CompileDX11Shaders)