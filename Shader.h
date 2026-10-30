#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>

class Shader
{
public:
    GLuint ID = 0;

    Shader(const std::string& vertexPath,
        const std::string& fragmentPath);

    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    void use() const;

    // 행렬을 GLSL uniform으로 전달
    void setMat4(const std::string& name,
        const glm::mat4& matrix) const;

    void setInt(const std::string& name, int value) const;
    
    void setFloat(
        const std::string& name,
        float value
    ) const;

    void setVec4(
        const std::string& name,
        const glm::vec4& value
    ) const;
};
