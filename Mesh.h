
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

    // GPU 스키닝용
    glm::uvec4 joints{ 0 };
    glm::vec4 weights{ 0.0f };
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

    void updateVertices(
        const std::vector<Vertex>& vertices
    );

private:
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLuint EBO = 0;

    GLsizei indexCount = 0;

    std::size_t vertexCount = 0;
};
