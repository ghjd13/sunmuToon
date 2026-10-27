
#pragma once

#include "Mesh.h"
#include "Shader.h"
#include "Texture.h"

#include <glm/glm.hpp>

#include <memory>
#include <string>
#include <vector>
#include <cstddef>

class GltfModel
{
public:
    explicit GltfModel(const std::string& path);
    ~GltfModel();

    GltfModel(const GltfModel&) = delete;
    GltfModel& operator=(const GltfModel&) = delete;

    void draw(
        const Shader& shader,
        const glm::mat4& modelMatrix,
        float leftArmAngle = 0.0f
    ) const;

private:
    struct Part
    {
        std::unique_ptr<Mesh> mesh;
        std::shared_ptr<Texture2D> texture;

        glm::mat4 transform{ 1.0f };
        glm::vec4 baseColor{ 1.0f };

        int nodeIndex = -1;
        int skinIndex = -1;
    };

    struct Skin
    {
        std::vector<std::size_t> jointNodes;
        std::vector<glm::mat4> inverseBinds;
    };

    std::vector<Part> parts;
    std::vector<Skin> skins;

    std::vector<glm::mat4> nodeLocalTransforms;
    std::vector<int> nodeParents;

    int leftUpperArmNode = -1;

    GLuint skinUBO = 0;
};
