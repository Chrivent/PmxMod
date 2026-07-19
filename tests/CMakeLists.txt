add_executable(PmxModViewerTests
        Viewer/PostProcessTest.cpp
)

target_compile_features(PmxModViewerTests PRIVATE cxx_std_23)

target_link_libraries(PmxModViewerTests PRIVATE
        PmxMod::Viewer
        GTest::gtest_main
)

add_custom_command(TARGET PmxModViewerTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${DXCOMPILER_DLL}"
        "$<TARGET_FILE_DIR:PmxModViewerTests>"
        VERBATIM
)

include(GoogleTest)
gtest_discover_tests(PmxModViewerTests)
