add_library(PmxModShader STATIC)
add_library(PmxMod::Shader ALIAS PmxModShader)

target_sources(PmxModShader PRIVATE
        ShaderPackage.h ShaderPackage.cpp
)

target_compile_features(PmxModShader PUBLIC cxx_std_23)

target_include_directories(PmxModShader PUBLIC
        ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(PmxModShader
        PUBLIC PmxMod::Core
        PRIVATE nlohmann_json::nlohmann_json
)
