
#pragma once

#include <cgltf.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <array>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

class Skeleton
{
public:
    static constexpr std::size_t MaxJoints = 128;

    using Palette =
        std::array<glm::mat4, MaxJoints>;

    explicit Skeleton(const cgltf_data* data);

    // 매 프레임 자세 초기화
    void resetPose();

    // 이름으로 본 회전 지정
    bool setRotation(
        const std::string& boneName,
        const glm::quat& rotation
    );

    // 전체 본 월드 행렬 갱신
    void update();

    const glm::mat4& worldTransform(
        std::size_t nodeIndex
    ) const;

    // GPU에 보낼 스키닝 행렬 생성
    void buildPalette(
        std::size_t skinIndex,
        std::size_t meshNodeIndex,
        Palette& output
    ) const;

private:
    struct Skin
    {
        std::vector<std::size_t> jointNodes;
        std::vector<glm::mat4> inverseBinds;
    };

    std::vector<glm::mat4> baseLocal;
    std::vector<glm::mat4> poseOffsets;
    std::vector<glm::mat4> worldMatrices;

    std::vector<int> parents;
    std::vector<Skin> skins;

    std::unordered_map<std::string, std::size_t>
        boneNames;
};
