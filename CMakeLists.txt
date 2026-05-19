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
        src/Program/Config.h src/Program/Config.cpp
        src/Program/Controller.h src/Program/Controller.cpp
        src/Program/Sound.h src/Program/Sound.cpp
        src/Program/Panel/PanelCommandId.h
        src/Program/Panel/Panel.h
        src/Program/Panel/ScenePanel.h src/Program/Panel/ScenePanel.cpp
        src/Program/Panel/SoundPanel.h src/Program/Panel/SoundPanel.cpp
        
        src/Parser/PmxParser.h src/Parser/PmxParser.cpp
        src/Parser/VmdParser.h src/Parser/VmdParser.cpp
        src/Parser/BinaryReader.h src/Parser/BinaryReader.cpp

        src/Animation/AnimationKeySearch.h
        src/Animation/Bezier.h src/Animation/Bezier.cpp

        src/Animation/Model/Animation.h src/Animation/Model/Animation.cpp
        src/Animation/Model/AnimationBinder.h src/Animation/Model/AnimationBinder.cpp
        src/Animation/Model/AnimationBuilder.h src/Animation/Model/AnimationBuilder.cpp
        src/Animation/Model/AnimationTrackMap.h src/Animation/Model/AnimationTrackMap.cpp
        
        src/Animation/Camera/CameraAnimation.h src/Animation/Camera/CameraAnimation.cpp
        src/Animation/Camera/CameraAnimationBuilder.h src/Animation/Camera/CameraAnimationBuilder.cpp

        src/Model/Model.h src/Model/Model.cpp
        src/Model/ModelAnimator.h src/Model/ModelAnimator.cpp
        src/Model/ModelLoader.h src/Model/ModelLoader.cpp
        src/Model/ModelMorph.h src/Model/ModelMorph.cpp
        src/Model/ModelPose.h src/Model/ModelPose.cpp
        src/Model/ModelSkinning.h src/Model/ModelSkinning.cpp
        src/Model/Bone/IkSolver.h src/Model/Bone/IkSolver.cpp
        src/Model/Bone/Node.h src/Model/Bone/Node.cpp
        src/Model/Physics/Physics.h src/Model/Physics/Physics.cpp
        src/Model/Physics/RigidBody.h src/Model/Physics/RigidBody.cpp
        src/Model/Physics/Joint.h src/Model/Physics/Joint.cpp
        src/Model/Physics/MotionState.h src/Model/Physics/MotionState.cpp
        
        src/Util.h src/Util.cpp

        src/Viewer/Viewer.h src/Viewer/Viewer.cpp
        src/Viewer/Instance.h src/Viewer/Instance.cpp
        src/Viewer/TextureCache.h
        
        src/Viewer/Glfw/GlfwViewer.h src/Viewer/Glfw/GlfwViewer.cpp
        src/Viewer/Glfw/GlfwShaderFactory.h src/Viewer/Glfw/GlfwShaderFactory.cpp
        src/Viewer/Glfw/GlfwTextureCache.h src/Viewer/Glfw/GlfwTextureCache.cpp
        src/Viewer/Glfw/GlfwInstance.h src/Viewer/Glfw/GlfwInstance.cpp
        
        src/Viewer/Dx11/Dx11Viewer.h src/Viewer/Dx11/Dx11Viewer.cpp
        src/Viewer/Dx11/Dx11DescriptorFactory.h src/Viewer/Dx11/Dx11DescriptorFactory.cpp
        src/Viewer/Dx11/Dx11TextureCache.h src/Viewer/Dx11/Dx11TextureCache.cpp
        src/Viewer/Dx11/Dx11Instance.h src/Viewer/Dx11/Dx11Instance.cpp
)

target_link_libraries(PmxMod PUBLIC
        glfw
        glad::glad
        glm::glm

        d3d11
        dxgi
        d3dcompiler
        comctl32

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
