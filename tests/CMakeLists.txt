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
