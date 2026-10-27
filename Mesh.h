
#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <vector>
#include <cstdint>

class Mesh
{
public:
    Mesh(
        const std::vector<glm::vec3>& positions,
        const std::vector<std::uint32_t>& indices
    );

    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    void draw() const;

private:
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint EBO = 0;

    GLsizei indexCount = 0;
};
