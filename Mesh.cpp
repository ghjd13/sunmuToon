
#include "Mesh.h"

#include <cstddef>
#include <stdexcept>

Mesh::Mesh(
    const std::vector<Vertex>& vertices,
    const std::vector<std::uint32_t>& indices)
{
    if (vertices.empty() || indices.empty())
        throw std::runtime_error("Empty mesh");

    indexCount = static_cast<GLsizei>(indices.size());
    vertexCount = vertices.size();

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(
        GL_ARRAY_BUFFER,
        vertices.size() * sizeof(Vertex),
        vertices.data(),
        GL_DYNAMIC_DRAW
    );

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        indices.size() * sizeof(std::uint32_t),
        indices.data(),
        GL_STATIC_DRAW
    );

    // Position
    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE,
        sizeof(Vertex),
        (void*)offsetof(Vertex, position)
    );
    glEnableVertexAttribArray(0);

    // Normal
    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE,
        sizeof(Vertex),
        (void*)offsetof(Vertex, normal)
    );
    glEnableVertexAttribArray(1);

    // Texture Coordinates
    glVertexAttribPointer(
        2, 2, GL_FLOAT, GL_FALSE,
        sizeof(Vertex),
        (void*)offsetof(Vertex, texCoord)
    );
    glEnableVertexAttribArray(2);


    // location 3: Joint Indices
    glVertexAttribIPointer(
        3,
        4,
        GL_UNSIGNED_INT,
        sizeof(Vertex),
        (void*)offsetof(Vertex, joints)
    );
    glEnableVertexAttribArray(3);

    // location 4: Joint Weights
    glVertexAttribPointer(
        4,
        4,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        (void*)offsetof(Vertex, weights)
    );
    glEnableVertexAttribArray(4);


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


void Mesh::updateVertices(
    const std::vector<Vertex>& vertices)
{
    if (vertices.size() != vertexCount)
    {
        throw std::runtime_error(
            "Mesh vertex count mismatch"
        );
    }

    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    glBufferSubData(
        GL_ARRAY_BUFFER,
        0,
        static_cast<GLsizeiptr>(
            vertices.size() * sizeof(Vertex)
            ),
        vertices.data()
    );

    glBindBuffer(GL_ARRAY_BUFFER, 0);
}
