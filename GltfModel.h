#pragma once

#include "Mesh.h"
#include "Shader.h"
#include "Texture.h"
#include "Skeleton.h"

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

class GltfModel
{
public:
    explicit GltfModel(const std::string& path);
    ~GltfModel();

    GltfModel(const GltfModel&) = delete;
    GltfModel& operator=(const GltfModel&) = delete;

    void draw(const Shader& shader, const glm::mat4& modelMatrix) const;
    Skeleton& getSkeleton();

private:
    struct Part
    {
        std::unique_ptr<Mesh> mesh;
        std::shared_ptr<Texture2D> texture;
        glm::mat4 transform{1.0f};
        glm::vec4 baseColor{1.0f};
        int nodeIndex = -1;
        int skinIndex = -1;
    };

    std::vector<Part> parts;
    std::unique_ptr<Skeleton> skeleton;
    GLuint skinUBO = 0;
};
