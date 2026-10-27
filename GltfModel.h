
#pragma once

#include "Mesh.h"
#include "Shader.h"
#include "Texture.h"

#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <vector>

class GltfModel
{
public:
    explicit GltfModel(const std::string& path);

    void draw(
        const Shader& shader,
        const glm::mat4& modelMatrix
    ) const;

private:
    struct Part
    {
        std::unique_ptr<Mesh> mesh;
        std::shared_ptr<Texture2D> texture;

        glm::mat4 transform{ 1.0f };
        glm::vec4 baseColor{ 1.0f };
    };

    std::vector<Part> parts;
};
