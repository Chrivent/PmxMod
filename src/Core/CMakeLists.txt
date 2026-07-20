add_library(PmxModCore STATIC)
add_library(PmxMod::Core ALIAS PmxModCore)

target_sources(PmxModCore PRIVATE
        ../Util.h ../Util.cpp

        Parser/PmxParser.h Parser/PmxParser.cpp
        Parser/VmdParser.h Parser/VmdParser.cpp
        Parser/BinaryReader.h Parser/BinaryReader.cpp

        Animation/AnimationKeySearch.h
        Animation/Bezier.h Animation/Bezier.cpp

        Animation/Model/Animation.h Animation/Model/Animation.cpp
        Animation/Model/AnimationBinder.h Animation/Model/AnimationBinder.cpp
        Animation/Model/AnimationBuilder.h Animation/Model/AnimationBuilder.cpp
        Animation/Model/AnimationTrackMap.h Animation/Model/AnimationTrackMap.cpp

        Animation/Camera/Camera.h Animation/Camera/Camera.cpp
        Animation/Camera/CameraAnimation.h Animation/Camera/CameraAnimation.cpp
        Animation/Camera/CameraAnimationBuilder.h Animation/Camera/CameraAnimationBuilder.cpp

        Model/ModelTypes.h
        Model/Model.h Model/Model.cpp
        Model/ModelAnimator.h Model/ModelAnimator.cpp
        Model/ModelLoader.h Model/ModelLoader.cpp
        Model/ModelMorph.h Model/ModelMorph.cpp
        Model/ModelPose.h Model/ModelPose.cpp
        Model/ModelSkinning.h Model/ModelSkinning.cpp
        Model/ModelUpdater.h Model/ModelUpdater.cpp

        Model/Bone/IkSolver.h Model/Bone/IkSolver.cpp
        Model/Bone/Node.h Model/Bone/Node.cpp

        Model/Physics/Physics.h Model/Physics/Physics.cpp
        Model/Physics/RigidBody.h Model/Physics/RigidBody.cpp
        Model/Physics/Joint.h Model/Physics/Joint.cpp
        Model/Physics/MotionState.h Model/Physics/MotionState.cpp
)

target_compile_features(PmxModCore PUBLIC cxx_std_23)

target_include_directories(PmxModCore PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(PmxModCore PUBLIC
        glm::glm
        BulletDynamics
        BulletCollision
        LinearMath
        PRIVATE
        PmxMod::ProjectWarnings
)
