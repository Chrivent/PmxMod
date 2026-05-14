#pragma once

#include <string>
#include <filesystem>
#include <glad/glad.h>

namespace Chrivent::GlfwShaderHelper {
    // GLSL 셰이더 소스를 지정한 타입으로 컴파일한다.
    GLuint CompileShader(GLenum shaderType, const std::string& code);
    // 단일 GLSL 파일에서 버텍스/프래그먼트 분기를 위한 define 줄을 삽입한다.
    std::string InjectDefine(const std::string& src, const char* defineLine);
    // GLSL 파일을 읽어 버텍스/프래그먼트 셰이더 프로그램을 생성한다.
    GLuint CreateShader(const std::filesystem::path& file);
}
