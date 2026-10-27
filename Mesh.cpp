
#include "Mesh.h"

#include <stdexcept>

Mesh::Mesh(
    const std::vector<glm::vec3>& positions,
    const std::vector<std::uint32_t>& indices
)
{
    if (positions.empty() || indices.empty())
        throw std::runtime_error("Mesh data is empty");

    indexCount = static_cast<GLsizei>(indices.size());

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    // 정점 버퍼
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(
        GL_ARRAY_BUFFER,
        positions.size() * sizeof(glm::vec3),
        positions.data(),
        GL_STATIC_DRAW
    );

    // 인덱스 버퍼
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        indices.size() * sizeof(std::uint32_t),
        indices.data(),
        GL_STATIC_DRAW
    );

    // location = 0: Position
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(glm::vec3),
        nullptr
    );

    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

Mesh::~Mesh()
{
    glDeleteBuffers(1, &EBO);
    glDeleteBuffers(1, &VBO);
    glDeleteVertexArrays(1, &VAO);
}

void Mesh::draw() const
{
    glBindVertexArray(VAO);

    glDrawElements(
        GL_TRIANGLES,
        indexCount,
        GL_UNSIGNED_INT,
        nullptr
    );

    glBindVertexArray(0);
}
