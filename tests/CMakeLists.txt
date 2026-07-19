add_executable(PmxModViewerTests
        Viewer/DynamicBufferRingTest.cpp
        Viewer/PostProcessTest.cpp
        Viewer/ViewerTest.cpp
)

target_compile_features(PmxModViewerTests PRIVATE cxx_std_23)

target_link_libraries(PmxModViewerTests PRIVATE
        PmxMod::ProjectWarnings
        PmxMod::ViewerCommon
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(PmxModViewerTests)

add_executable(PmxModViewerApiTests
        Viewer/ApiDescriptorTest.cpp
        Viewer/GraphicsContractTest.cpp
        Viewer/ShaderCompilerTest.cpp
        ${PROJECT_SOURCE_DIR}/src/Program/Shader/InternalShaderCatalog.h
        ${PROJECT_SOURCE_DIR}/src/Program/Shader/InternalShaderCatalog.cpp
        ${PROJECT_SOURCE_DIR}/src/Program/Shader/ShaderPackage.h
        ${PROJECT_SOURCE_DIR}/src/Program/Shader/ShaderPackage.cpp
)

target_compile_features(PmxModViewerApiTests PRIVATE cxx_std_23)

target_compile_definitions(PmxModViewerApiTests PRIVATE
        PMXMOD_RESOURCE_SOURCE_DIR="${PROJECT_SOURCE_DIR}/resource"
)

target_link_libraries(PmxModViewerApiTests PRIVATE
        PmxMod::ProjectWarnings
        PmxMod::Viewer
        GTest::gtest_main
        nlohmann_json::nlohmann_json
)

add_custom_command(TARGET PmxModViewerApiTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${DXCOMPILER_DLL}"
        "$<TARGET_FILE_DIR:PmxModViewerApiTests>"
        VERBATIM
)

gtest_discover_tests(PmxModViewerApiTests)
