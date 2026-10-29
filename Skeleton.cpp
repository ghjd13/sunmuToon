
#include "Skeleton.h"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <utility>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <nlohmann/json.hpp>

Skeleton::Skeleton(const cgltf_data* data)
{
    if (!data)
        throw std::runtime_error("Null glTF data");

    const std::size_t nodeCount = data->nodes_count;

    baseLocal.resize(nodeCount);
    poseOffsets.assign(nodeCount, glm::mat4(1.0f));
    worldMatrices.resize(nodeCount);
    parents.resize(nodeCount, -1);

    // 1. 노드 계층과 기본 변환 읽기
    for (std::size_t i = 0; i < nodeCount; ++i)
    {
        const cgltf_node& node = data->nodes[i];

        float matrix[16]{};
        cgltf_node_transform_local(&node, matrix);

        baseLocal[i] = glm::make_mat4(matrix);

        if (node.parent)
        {
            parents[i] = static_cast<int>(
                node.parent - data->nodes
                );
        }

        if (node.name)
        {
            boneNames.emplace(node.name, i);
        }
    }

    // 2. Skin과 Inverse Bind Matrix 읽기
    for (std::size_t s = 0;
        s < data->skins_count; ++s)
    {
        const cgltf_skin& source = data->skins[s];

        if (source.joints_count > MaxJoints)
            throw std::runtime_error(
                "Too many joints for UBO"
            );

        if (source.inverse_bind_matrices &&
            source.inverse_bind_matrices->count <
            source.joints_count)
        {
            throw std::runtime_error(
                "Invalid inverse bind matrix count"
            );
        }

        Skin skin;

        for (std::size_t j = 0;
            j < source.joints_count; ++j)
        {
            auto index =
                source.joints[j] - data->nodes;

            if (index < 0 ||
                static_cast<std::size_t>(index) >=
                nodeCount)
            {
                throw std::runtime_error(
                    "Invalid joint node"
                );
            }

            skin.jointNodes.push_back(
                static_cast<std::size_t>(index)
            );

            glm::mat4 inverseBind(1.0f);

            if (source.inverse_bind_matrices)
            {
                float values[16]{};

                if (!cgltf_accessor_read_float(
                    source.inverse_bind_matrices,
                    j, values, 16))
                {
                    throw std::runtime_error(
                        "Cannot read inverse bind matrix"
                    );
                }

                inverseBind = glm::make_mat4(values);
            }

            skin.inverseBinds.push_back(inverseBind);
        }

        skins.push_back(std::move(skin));
    }

    // 3. VRM 0.x / VRM 1.0 Humanoid 매핑 읽기
    // cgltf는 미지원 확장 JSON을 data_extensions에 보관합니다.
    const auto addHumanoidBone = [&](const std::string& role,
                                      const nlohmann::json& indexJson)
    {
        if (!indexJson.is_number_integer())
            return;

        const std::int64_t index = indexJson.get<std::int64_t>();
        if (index < 0 ||
            static_cast<std::uint64_t>(index) >= nodeCount)
            return;

        humanoidBones.emplace(role, static_cast<std::size_t>(index));
    };

    std::string vrmVersion = "None";

    for (cgltf_size i = 0; i < data->data_extensions_count; ++i)
    {
        const cgltf_extension& extension = data->data_extensions[i];
        if (!extension.name || !extension.data)
            continue;

        const bool isVrm1 = std::strcmp(extension.name, "VRMC_vrm") == 0;
        const bool isVrm0 = std::strcmp(extension.name, "VRM") == 0;
        if (!isVrm1 && !isVrm0)
            continue;

        try
        {
            const auto json = nlohmann::json::parse(extension.data);
            auto humanoid = json.find("humanoid");
            if (humanoid == json.end() || !humanoid->is_object())
                continue;

            auto bones = humanoid->find("humanBones");
            if (bones == humanoid->end())
                continue;

            if (isVrm1 && bones->is_object())
            {
                // VRM 1.0: { "head": { "node": 10 }, ... }
                for (auto it = bones->begin(); it != bones->end(); ++it)
                {
                    const auto& boneInfo = it.value();
                    if (!boneInfo.is_object())
                        continue;

                    auto node = boneInfo.find("node");
                    if (node != boneInfo.end())
                        addHumanoidBone(it.key(), *node);
                }
                vrmVersion = "1.0";
            }
            else if (isVrm0 && bones->is_array())
            {
                // VRM 0.x: [{ "bone": "head", "node": 10 }, ...]
                for (const auto& boneInfo : *bones)
                {
                    if (!boneInfo.is_object())
                        continue;

                    auto role = boneInfo.find("bone");
                    auto node = boneInfo.find("node");
                    if (role != boneInfo.end() && role->is_string() &&
                        node != boneInfo.end())
                    {
                        addHumanoidBone(role->get<std::string>(), *node);
                    }
                }
                vrmVersion = "0.x";
            }
        }
        catch (const nlohmann::json::exception& e)
        {
            throw std::runtime_error(
                std::string("VRM humanoid JSON parse failed: ") + e.what()
            );
        }
    }

    std::cout << "VRM version: " << vrmVersion
              << ", mapped humanoid bones: " << humanoidBones.size()
              << '\n';

    for (const char* role : { "head", "leftUpperArm", "rightUpperArm" })
    {
        std::cout << "  " << role << " -> node "
                  << humanoidNodeIndex(role) << '\n';
    }

    update();
}

void Skeleton::resetPose()
{
    std::fill(
        poseOffsets.begin(),
        poseOffsets.end(),
        glm::mat4(1.0f)
    );
}

bool Skeleton::setRotation(
    const std::string& boneName,
    const glm::quat& rotation)
{
    auto it = boneNames.find(boneName);

    if (it == boneNames.end())
        return false;

    poseOffsets[it->second] =
        glm::mat4_cast(glm::normalize(rotation));

    return true;
}

bool Skeleton::setHumanoidRotation(
    const std::string& role,
    const glm::quat& rotation)
{
    auto it = humanoidBones.find(role);
    if (it == humanoidBones.end())
        return false;

    poseOffsets[it->second] =
        glm::mat4_cast(glm::normalize(rotation));
    return true;
}

int Skeleton::humanoidNodeIndex(const std::string& role) const
{
    auto it = humanoidBones.find(role);
    if (it == humanoidBones.end())
        return -1;
    return static_cast<int>(it->second);
}

void Skeleton::update()
{
    std::vector<int> state(baseLocal.size(), 0);

    std::function<void(std::size_t)> visit;

    visit = [&](std::size_t index)
        {
            if (state[index] == 2)
                return;

            if (state[index] == 1)
                throw std::runtime_error(
                    "Cyclic skeleton hierarchy"
                );

            state[index] = 1;

            glm::mat4 local =
                baseLocal[index] * poseOffsets[index];

            int parent = parents[index];

            if (parent >= 0)
            {
                if (static_cast<std::size_t>(parent) >=
                    baseLocal.size())
                    throw std::runtime_error(
                        "Invalid parent node"
                    );

                visit(static_cast<std::size_t>(parent));

                worldMatrices[index] =
                    worldMatrices[parent] * local;
            }
            else
            {
                worldMatrices[index] = local;
            }

            state[index] = 2;
        };

    for (std::size_t i = 0; i < baseLocal.size(); ++i)
        visit(i);
}

const glm::mat4& Skeleton::worldTransform(
    std::size_t nodeIndex) const
{
    return worldMatrices.at(nodeIndex);
}

void Skeleton::buildPalette(
    std::size_t skinIndex,
    std::size_t meshNodeIndex,
    Palette& output) const
{
    output.fill(glm::mat4(1.0f));

    const Skin& skin = skins.at(skinIndex);

    glm::mat4 inverseMesh =
        glm::inverse(worldMatrices.at(meshNodeIndex));

    for (std::size_t j = 0;
        j < skin.jointNodes.size(); ++j)
    {
        output[j] =
            inverseMesh *
            worldMatrices.at(skin.jointNodes[j]) *
            skin.inverseBinds[j];
    }
}