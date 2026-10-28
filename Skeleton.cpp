
#include "Skeleton.h"

#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <utility>

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
