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
        external/miniaudio.h
        external/stb_image.h
        src/MMDUtil.cpp         src/MMDUtil.h
        src/MMDIkSolver.cpp     src/MMDIkSolver.h
        src/MMDModel.cpp        src/MMDModel.h
        src/MMDNode.cpp         src/MMDNode.h
        src/MMDPhysics.cpp      src/MMDPhysics.h
        src/MMDReader.cpp       src/MMDReader.h
        src/VMDAnimation.cpp    src/VMDAnimation.h
        viewer/GLFWViewer.cpp   viewer/GLFWViewer.h
        viewer/DX11Viewer.cpp   viewer/DX11Viewer.h
)

target_link_libraries(PmxMod PUBLIC
        glfw
        glad::glad
        glm::glm
        BulletDynamics
        BulletCollision
        LinearMath

        d3d11
        dxgi
        d3dcompiler
)
target_compile_definitions(PmxMod PUBLIC)

set(RESOURCE_DIR ${CMAKE_SOURCE_DIR}/resource)
add_custom_target(SyncResources
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:PmxMod>/resource
        COMMAND ${CMAKE_COMMAND} -E copy_directory  ${RESOURCE_DIR} $<TARGET_FILE_DIR:PmxMod>/resource
        VERBATIM
)
add_dependencies(PmxMod SyncResources)



set(SHADER_DX11_SRC_DIR ${CMAKE_SOURCE_DIR}/resource/shader_DX11)
set(SHADER_DX11_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shader_DX11)
find_program(FXC_EXE fxc)
function(add_fxc_shader OUT_VAR SRC PROFILE ENTRY)
    get_filename_component(SRC_NAME ${SRC} NAME_WE)
    set(OUT_FILE ${SHADER_DX11_OUT_DIR}/${SRC_NAME}.${PROFILE}.${ENTRY}.cso)
    add_custom_command(
            OUTPUT ${OUT_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_DX11_OUT_DIR}
            COMMAND ${FXC_EXE} /nologo
            /T ${PROFILE}
            /E ${ENTRY}
            /Fo ${OUT_FILE}
            ${SRC}
            DEPENDS ${SRC}
            VERBATIM
    )
    set(${OUT_VAR} "${OUT_FILE}" PARENT_SCOPE)
endfunction()

add_fxc_shader(MMD_VS_CSO  ${SHADER_DX11_SRC_DIR}/mmd.fx               vs_5_0 VSMain)
add_fxc_shader(MMD_PS_CSO  ${SHADER_DX11_SRC_DIR}/mmd.fx               ps_5_0 PSMain)
add_fxc_shader(EDGE_VS_CSO ${SHADER_DX11_SRC_DIR}/mmd_edge.fx          vs_5_0 VSMain)
add_fxc_shader(EDGE_PS_CSO ${SHADER_DX11_SRC_DIR}/mmd_edge.fx          ps_5_0 PSMain)
add_fxc_shader(GS_VS_CSO   ${SHADER_DX11_SRC_DIR}/mmd_ground_shadow.fx vs_5_0 VSMain)
add_fxc_shader(GS_PS_CSO   ${SHADER_DX11_SRC_DIR}/mmd_ground_shadow.fx ps_5_0 PSMain)

add_custom_target(CompileDX11Shaders
        DEPENDS
        ${MMD_VS_CSO} ${MMD_PS_CSO}
        ${EDGE_VS_CSO} ${EDGE_PS_CSO}
        ${GS_VS_CSO} ${GS_PS_CSO}
)

add_custom_target(SyncDX11Shaders
        DEPENDS CompileDX11Shaders
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:PmxMod>/resource/shader_DX11
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MMD_VS_CSO}  $<TARGET_FILE_DIR:PmxMod>/resource/shader_DX11/mmd.vs.cso
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${MMD_PS_CSO}  $<TARGET_FILE_DIR:PmxMod>/resource/shader_DX11/mmd.ps.cso
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EDGE_VS_CSO} $<TARGET_FILE_DIR:PmxMod>/resource/shader_DX11/mmd_edge.vs.cso
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EDGE_PS_CSO} $<TARGET_FILE_DIR:PmxMod>/resource/shader_DX11/mmd_edge.ps.cso
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${GS_VS_CSO}   $<TARGET_FILE_DIR:PmxMod>/resource/shader_DX11/mmd_ground_shadow.vs.cso
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${GS_PS_CSO}   $<TARGET_FILE_DIR:PmxMod>/resource/shader_DX11/mmd_ground_shadow.ps.cso
        VERBATIM
)
add_dependencies(PmxMod CompileDX11Shaders SyncDX11Shaders)