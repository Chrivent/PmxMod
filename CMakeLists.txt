cmake_minimum_required(VERSION 3.20)

# (vcpkg 쓰는 경우) toolchain 지정
set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")

project(PmxMod LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 옵션으로 on/off
option(BUILD_VIEWER  "Build standalone OpenGL viewer" ON)
option(BUILD_BRIDGE  "Build JNI bridge DLL"          ON)

# ---------------------- 코어: pmxcore (STATIC) ----------------------
# OpenGL/GLFW/Shader/Model(렌더)/**main.cpp는 포함하지 마세요**
set(CORE_SOURCES
        base/File.cpp            base/File.h
        base/Path.cpp            base/Path.h
        base/UnicodeUtil.cpp     base/UnicodeUtil.h
        src/MMDCamera.cpp        src/MMDCamera.h
        src/MMDIkSolver.cpp      src/MMDIkSolver.h
        src/MMDMaterial.cpp      src/MMDMaterial.h
        src/MMDModel.cpp         src/MMDModel.h
        src/MMDMorph.cpp         src/MMDMorph.h
        src/MMDNode.cpp          src/MMDNode.h
        src/MMDPhysics.cpp       src/MMDPhysics.h
        src/PMXFile.cpp          src/PMXFile.h
        src/VMDAnimation.cpp     src/VMDAnimation.h
        src/VMDCameraAnimation.cpp src/VMDCameraAnimation.h
        src/VMDFile.cpp          src/VMDFile.h
        external/stb_image.h
)

find_package(glm CONFIG REQUIRED)
find_package(Bullet CONFIG REQUIRED)

add_library(pmxcore STATIC ${CORE_SOURCES})
target_link_libraries(pmxcore PUBLIC glm::glm BulletDynamics BulletCollision LinearMath)
target_compile_definitions(pmxcore PRIVATE NOMINMAX)

# ---------------------- 뷰어 실행파일: PmxViewer ----------------------
if(BUILD_VIEWER)
    find_package(glfw3 CONFIG REQUIRED)
    find_package(glad  CONFIG REQUIRED)

    add_executable(PmxViewer
            main.cpp
            AppContext.cpp AppContext.h
            MMDShader.cpp  MMDShader.h
            Model.cpp      Model.h
            external/miniaudio.h
    )
    target_link_libraries(PmxViewer PRIVATE
            pmxcore
            glfw
            glad::glad
            glm::glm            # (필요시)
            BulletDynamics BulletCollision LinearMath # (pmxcore가 링크하므로 보통 불필요하지만 안전하게)
    )
    target_compile_definitions(PmxViewer PRIVATE NOMINMAX MINIAUDIO_IMPLEMENTATION)

    # 리소스 복사
    set(RESOURCE_DIR ${CMAKE_SOURCE_DIR}/resource)
    add_custom_command(TARGET PmxViewer POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${RESOURCE_DIR} $<TARGET_FILE_DIR:PmxViewer>/resource
    )
endif()

# ---------------------- JNI 브리지 DLL: pmxbridge ----------------------
if(BUILD_BRIDGE)
    find_package(JNI REQUIRED)

    add_library(pmxbridge SHARED
            jni_bridge.cpp
            external/miniaudio.h     # 음악 싱크를 네이티브에서 쓸 경우에만
    )
    target_link_libraries(pmxbridge PRIVATE pmxcore)
    target_include_directories(pmxbridge PRIVATE ${JNI_INCLUDE_DIRS})
    target_compile_definitions(pmxbridge PRIVATE NOMINMAX MINIAUDIO_IMPLEMENTATION)
    # JNIEXPORT로 심볼이 노출되므로 별도 export 설정 불필요
endif()

set(FORGE_RUN_DIR "C:/Users/Ha Yechan/Desktop/PmxSteveMod/run")

add_custom_command(TARGET pmxbridge POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:pmxbridge> "${FORGE_RUN_DIR}"
)
