
#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <vector>
#include <cstdint>

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoord;
};

class Mesh
{
public:
    Mesh(
        const std::vector<Vertex>& vertices,
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
