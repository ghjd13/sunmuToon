
#include "Shader.h"

#include <glm/gtc/type_ptr.hpp>

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace
{
    std::string readFile(const std::string& path)
    {
        std::ifstream file(path);

        if (!file.is_open()) {
            throw std::runtime_error(
                "Cannot open shader file: " + path
            );
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    GLuint compileShader(GLenum type,
        const std::string& source)
    {
        GLuint shader = glCreateShader(type);
        const char* code = source.c_str();

        glShaderSource(shader, 1, &code, nullptr);
        glCompileShader(shader);

        GLint success = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

        if (!success) {
            char log[2048] = {};
            glGetShaderInfoLog(
                shader, sizeof(log), nullptr, log
            );

            glDeleteShader(shader);

            throw std::runtime_error(
                std::string("Shader compilation failed: ") + log
            );
        }

        return shader;
    }
}

Shader::Shader(const std::string& vertexPath,
    const std::string& fragmentPath)
{
    std::string vertexCode = readFile(vertexPath);
    std::string fragmentCode = readFile(fragmentPath);

    GLuint vertex = compileShader(
        GL_VERTEX_SHADER, vertexCode
    );

    GLuint fragment = 0;

    try {
        fragment = compileShader(
            GL_FRAGMENT_SHADER, fragmentCode
        );

        ID = glCreateProgram();

        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        glLinkProgram(ID);

        GLint success = GL_FALSE;
        glGetProgramiv(ID, GL_LINK_STATUS, &success);

        if (!success) {
            char log[2048] = {};
            glGetProgramInfoLog(
                ID, sizeof(log), nullptr, log
            );

            throw std::runtime_error(
                std::string("Shader linking failed: ") + log
            );
        }
    }
    catch (...) {
        glDeleteShader(vertex);

        if (fragment != 0)
            glDeleteShader(fragment);

        if (ID != 0) {
            glDeleteProgram(ID);
            ID = 0;
        }

        throw;
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

Shader::~Shader()
{
    if (ID != 0)
        glDeleteProgram(ID);
}

void Shader::use() const
{
    glUseProgram(ID);
}


void Shader::setMat4(
    const std::string& name,
    const glm::mat4& matrix
) const
{
    GLint location = glGetUniformLocation(
        ID, name.c_str()
    );

    glUniformMatrix4fv(
        location,
        1,
        GL_FALSE,
        glm::value_ptr(matrix)
    );
}


void Shader::setInt(
    const std::string& name,
    int value
) const
{
    glUniform1i(
        glGetUniformLocation(ID, name.c_str()),
        value
    );
}

void Shader::setVec4(
    const std::string& name,
    const glm::vec4& value
) const
{
    glUniform4fv(
        glGetUniformLocation(ID, name.c_str()),
        1,
        glm::value_ptr(value)
    );
}


void Shader::setFloat(
    const std::string& name,
    float value
) const
{
    GLint location = glGetUniformLocation(
        ID,
        name.c_str()
    );

    glUniform1f(location, value);
}
