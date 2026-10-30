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
        // MToon 그림자용 텍스처
        std::shared_ptr<Texture2D> shadeTexture;
        // glTF 노멀맵
        std::shared_ptr<Texture2D> normalTexture;
        float normalScale = 1.0f;

        glm::mat4 transform{1.0f};
        glm::vec4 baseColor{1.0f};

        // VRM 1.0 MToon 파라미터
        bool hasMToon = false;

        glm::vec4 mtoonShadeColor{
            0.0f, 0.0f, 0.0f, 1.0f
        };

        float mtoonShadingShift = 0.0f;
        float mtoonShadingToony = 0.9f;

        int nodeIndex = -1;
        int skinIndex = -1;

        // Morph 적용 전의 원본 정점
        std::vector<Vertex> baseVertices;

        // 눈 감기와 입 벌리기의 POSITION 변화량
        std::vector<glm::vec3> blinkDeltas;
        std::vector<glm::vec3> mouthDeltas;
        std::vector<glm::vec3> happyDeltas;

        // glTF 알파 렌더링 모드
        enum class AlphaMode
        {
            Opaque = 0,
            Mask = 1,
            Blend = 2
        };

        AlphaMode alphaMode = AlphaMode::Opaque;
        float alphaCutoff = 0.5f;

        // MToon 투명 렌더링 옵션
        bool transparentWithZWrite = false;
        int renderQueueOffset = 0;
    };

    std::vector<Part> parts;
    std::unique_ptr<Skeleton> skeleton;
    GLuint skinUBO = 0;

    float lastBlinkWeight = -1.0f;
    float lastMouthWeight = -1.0f;
    float lastHappyWeight = -1.0f;
};
