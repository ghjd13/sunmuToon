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

    void updateFace(
        float blinkWeight,
        float mouthWeight,
        float happyWeight
    );

private:
    struct Part
    {
        std::unique_ptr<Mesh> mesh;
        std::shared_ptr<Texture2D> texture;
        glm::mat4 transform{1.0f};
        glm::vec4 baseColor{1.0f};
        int nodeIndex = -1;
        int skinIndex = -1;

        // Morph 적용 전의 원본 정점
        std::vector<Vertex> baseVertices;

        // 눈 감기와 입 벌리기의 POSITION 변화량
        std::vector<glm::vec3> blinkDeltas;
        std::vector<glm::vec3> mouthDeltas;
        std::vector<glm::vec3> happyDeltas;
    };

    std::vector<Part> parts;
    std::unique_ptr<Skeleton> skeleton;
    GLuint skinUBO = 0;

    float lastBlinkWeight = -1.0f;
    float lastMouthWeight = -1.0f;
    float lastHappyWeight = -1.0f;
};
