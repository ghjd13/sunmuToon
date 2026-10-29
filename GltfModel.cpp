#include <cgltf.h>

#include "GltfModel.h"

#include <glm/gtc/type_ptr.hpp>

#include <nlohmann/json.hpp>
#include <cstring>

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace
{
    std::shared_ptr<Texture2D> loadImage(
        const cgltf_image* image,
        const std::string& modelPath)
    {
        if (!image)
            return nullptr;

        // GLB 내부 이미지
        if (image->buffer_view)
        {
            const cgltf_buffer_view* view =
                image->buffer_view;

            const auto* bytes =
                static_cast<const unsigned char*>(
                    view->buffer->data
                    ) + view->offset;

            return std::make_shared<Texture2D>(
                bytes, view->size
            );
        }

        // 외부 이미지 파일
        if (image->uri)
        {
            std::string uri = image->uri;

            // Data URI는 아직 미지원
            if (uri.rfind("data:", 0) == 0) {
                std::cerr
                    << "Data URI image not supported yet\n";
                return nullptr;
            }

            std::filesystem::path imagePath =
                std::filesystem::path(modelPath)
                .parent_path() / uri;

            return std::make_shared<Texture2D>(
                imagePath.string()
            );
        }

        return nullptr;
    }
}

GltfModel::GltfModel(const std::string& path)
{
    cgltf_options options{};
    cgltf_data* rawData = nullptr;

    cgltf_result result = cgltf_parse_file(
        &options, path.c_str(), &rawData
    );

    if (result != cgltf_result_success)
        throw std::runtime_error("glTF parse failed");

    std::unique_ptr<cgltf_data, decltype(&cgltf_free)>
        data(rawData, &cgltf_free);

    result = cgltf_load_buffers(
        &options, data.get(), path.c_str()
    );

    if (result != cgltf_result_success)
        throw std::runtime_error("Buffer load failed");

    skeleton = std::make_unique<Skeleton>(data.get());

    std::cout << "Skeleton data initialized\n";


    // ===== Morph Target 정보 검사 =====

    std::cout << "\n===== Morph Target Information =====\n";

    for (cgltf_size m = 0; m < data->meshes_count; ++m)
    {
        const cgltf_mesh& mesh = data->meshes[m];

        std::cout << "Mesh " << m << ": "
            << (mesh.name ? mesh.name : "Unnamed")
            << '\n';

        for (cgltf_size p = 0;
            p < mesh.primitives_count; ++p)
        {
            const cgltf_primitive& primitive =
                mesh.primitives[p];

            std::cout << "  Primitive " << p
                << " | Morph Targets: "
                << primitive.targets_count
                << '\n';

            // 첫 번째 Morph Target의 POSITION 데이터 확인
            if (primitive.targets_count > 0)
            {
                const cgltf_morph_target& target =
                    primitive.targets[0];

                for (cgltf_size a = 0;
                    a < target.attributes_count; ++a)
                {
                    const cgltf_attribute& attribute =
                        target.attributes[a];

                    if (attribute.type ==
                        cgltf_attribute_type_position &&
                        attribute.data)
                    {
                        std::cout
                            << "    Target 0 POSITION count: "
                            << attribute.data->count << '\n';
                    }
                }
            }
        }
    }

    std::cout << "\n===== VRM Expressions =====\n";

    for (cgltf_size e = 0;
        e < data->data_extensions_count; ++e)
    {
        const cgltf_extension& extension =
            data->data_extensions[e];

        if (!extension.name || !extension.data)
            continue;

        if (std::strcmp(extension.name, "VRMC_vrm") != 0)
            continue;

        auto vrm = nlohmann::json::parse(
            extension.data, nullptr, false
        );

        if (!vrm.is_object())
        {
            std::cout << "Invalid VRM expression JSON\n";
            continue;
        }

        auto expressions = vrm.find("expressions");

        if (expressions == vrm.end() ||
            !expressions->is_object())
        {
            std::cout << "No expressions found\n";
            continue;
        }

        for (const char* groupName : { "preset", "custom" })
        {
            auto group = expressions->find(groupName);

            if (group == expressions->end() ||
                !group->is_object())
                continue;

            for (auto it = group->begin();
                it != group->end(); ++it)
            {
                if (!it.value().is_object())
                    continue;

                const auto& expression = it.value();

                auto countBinds = [&](const char* name)
                    {
                        auto found = expression.find(name);

                        return (found != expression.end() &&
                            found->is_array())
                            ? found->size()
                            : std::size_t{ 0 };
                    };

                std::cout
                    << "  " << groupName << "/" << it.key()
                    << " | Morph: "
                    << countBinds("morphTargetBinds")
                    << " | Material: "
                    << countBinds("materialColorBinds")
                    << " | UV: "
                    << countBinds("textureTransformBinds")
                    << '\n';

                // 표정별 Morph Target 연결 정보 확인
                auto morphBinds = expression.find("morphTargetBinds");

                if (morphBinds != expression.end() &&
                    morphBinds->is_array())
                {
                    for (const auto& bind : *morphBinds)
                    {
                        if (!bind.is_object() ||
                            !bind.contains("node") ||
                            !bind.contains("index") ||
                            !bind["node"].is_number_integer() ||
                            !bind["index"].is_number_integer())
                        {
                            continue;
                        }

                        int nodeIndex = bind["node"].get<int>();
                        int targetIndex = bind["index"].get<int>();

                        float weight = 1.0f;
                        if (bind.contains("weight") &&
                            bind["weight"].is_number())
                        {
                            weight = bind["weight"].get<float>();
                        }

                        if (nodeIndex < 0 ||
                            static_cast<cgltf_size>(nodeIndex) >=
                            data->nodes_count)
                        {
                            std::cout << "    Invalid node\n";
                            continue;
                        }

                        const cgltf_node& targetNode =
                            data->nodes[nodeIndex];

                        std::cout
                            << "    Node: " << nodeIndex
                            << " | Target: " << targetIndex
                            << " | Weight: " << weight
                            << " | Mesh: "
                            << (targetNode.mesh && targetNode.mesh->name
                                ? targetNode.mesh->name
                                : "Unnamed")
                            << '\n';
                    }
                }

            }
        }
    }

    std::cout << "===============================\n";


    std::cout << "\n===== Model Information =====\n";

    std::cout << "Nodes: "
        << data->nodes_count << '\n';

    std::cout << "Meshes: "
        << data->meshes_count << '\n';

    std::cout << "Materials: "
        << data->materials_count << '\n';

    std::cout << "Textures: "
        << data->textures_count << '\n';

    std::cout << "Skins: "
        << data->skins_count << '\n';

    // 스켈레톤 정보 출력
    for (cgltf_size i = 0; i < data->skins_count; ++i)
    {
        const cgltf_skin& skin = data->skins[i];

        std::cout << "Skin " << i
            << " - Joint Count: "
            << skin.joints_count << '\n';

        for (cgltf_size j = 0; j < skin.joints_count; ++j)
        {
            const cgltf_node* joint = skin.joints[j];

            if (!joint)
                continue;

            std::cout << "  Joint " << j << ": "
                << (joint->name ? joint->name : "Unnamed")
                << '\n';
        }
    }

    std::cout << "=============================\n";

    // 동일한 이미지를 여러 번 로드하지 않음
    std::unordered_map<
        const cgltf_image*,
        std::shared_ptr<Texture2D>
    > textureCache;

    for (cgltf_size n = 0; n < data->nodes_count; ++n)
    {
        const cgltf_node& node = data->nodes[n];

        if (!node.mesh)
            continue;

        float worldMatrix[16];

        cgltf_node_transform_world(
            &node, worldMatrix
        );

        glm::mat4 transform =
            glm::make_mat4(worldMatrix);

        for (cgltf_size p = 0;
            p < node.mesh->primitives_count;
            ++p)
        {
            const cgltf_primitive& primitive =
                node.mesh->primitives[p];

            if (primitive.type !=
                cgltf_primitive_type_triangles)
                continue;


            const cgltf_accessor* joints =
                cgltf_find_accessor(
                    &primitive,
                    cgltf_attribute_type_joints,
                    0
                );

            const cgltf_accessor* weights =
                cgltf_find_accessor(
                    &primitive,
                    cgltf_attribute_type_weights,
                    0
                );

            std::cout << "\n--- Skinning Info ---\n";

            std::cout << "Skinned Node: "
                << (node.skin ? "Yes" : "No") << '\n';

            std::cout << "JOINTS_0 Count: "
                << (joints ? joints->count : 0) << '\n';

            std::cout << "WEIGHTS_0 Count: "
                << (weights ? weights->count : 0) << '\n';

            if (node.skin) {
                std::cout << "Skin Joint Count: "
                    << node.skin->joints_count << '\n';

                std::cout << "Inverse Bind Matrices: "
                    << (node.skin->inverse_bind_matrices
                        ? "Present" : "Not specified")
                    << '\n';
            }


            const cgltf_accessor* pos =
                cgltf_find_accessor(
                    &primitive,
                    cgltf_attribute_type_position,
                    0
                );

            const cgltf_accessor* normal =
                cgltf_find_accessor(
                    &primitive,
                    cgltf_attribute_type_normal,
                    0
                );

            int uvSet = 0;

            if (primitive.material &&
                primitive.material
                ->has_pbr_metallic_roughness)
            {
                uvSet = primitive.material
                    ->pbr_metallic_roughness
                    .base_color_texture.texcoord;
            }

            const cgltf_accessor* uv =
                cgltf_find_accessor(
                    &primitive,
                    cgltf_attribute_type_texcoord,
                    uvSet
                );

            if (!pos || pos->type != cgltf_type_vec3)
                continue;

            std::vector<Vertex> vertices;
            std::vector<std::uint32_t> indices;

            vertices.reserve(pos->count);

            for (cgltf_size i = 0;
                i < pos->count;
                ++i)
            {
                Vertex v{};

                float position[3]{};
                float normalData[3]{ 0.0f, 1.0f, 0.0f };
                float uvData[2]{};

                if (!cgltf_accessor_read_float(
                    pos, i, position, 3))
                    throw std::runtime_error(
                        "Invalid position accessor"
                    );

                if (normal && i < normal->count) {
                    if (!cgltf_accessor_read_float(
                        normal, i, normalData, 3))
                        throw std::runtime_error(
                            "Invalid normal accessor"
                        );
                }

                if (uv && i < uv->count) {
                    if (!cgltf_accessor_read_float(
                        uv, i, uvData, 2))
                        throw std::runtime_error(
                            "Invalid UV accessor"
                        );
                }

                v.position = glm::vec3(
                    position[0],
                    position[1],
                    position[2]
                );

                v.normal = glm::vec3(
                    normalData[0],
                    normalData[1],
                    normalData[2]
                );

                v.texCoord = glm::vec2(
                    uvData[0],
                    uvData[1]
                );


                // 정점별 본 인덱스와 가중치 읽기
                if (node.skin && joints && weights)
                {
                    if (joints->count != pos->count ||
                        weights->count != pos->count)
                    {
                        throw std::runtime_error(
                            "Skin attribute count mismatch"
                        );
                    }

                    cgltf_uint jointData[4]{};
                    cgltf_float weightData[4]{};

                    if (!cgltf_accessor_read_uint(
                        joints, i, jointData, 4))
                    {
                        throw std::runtime_error(
                            "Failed to read joint indices"
                        );
                    }

                    if (!cgltf_accessor_read_float(
                        weights, i, weightData, 4))
                    {
                        throw std::runtime_error(
                            "Failed to read joint weights"
                        );
                    }

                    for (int j = 0; j < 4; ++j)
                    {
                        if (jointData[j] >= node.skin->joints_count)
                        {
                            throw std::runtime_error(
                                "Joint index out of range"
                            );
                        }
                    }

                    v.joints = glm::uvec4(
                        jointData[0],
                        jointData[1],
                        jointData[2],
                        jointData[3]
                    );

                    v.weights = glm::vec4(
                        weightData[0],
                        weightData[1],
                        weightData[2],
                        weightData[3]
                    );
                }


                vertices.push_back(v);
            }

            // 인덱스
            if (primitive.indices)
            {
                const cgltf_accessor* acc =
                    primitive.indices;

                for (cgltf_size i = 0;
                    i < acc->count;
                    ++i)
                {
                    cgltf_size index =
                        cgltf_accessor_read_index(acc, i);

                    if (index >= vertices.size())
                        throw std::runtime_error(
                            "Invalid mesh index"
                        );

                    indices.push_back(
                        static_cast<std::uint32_t>(index)
                    );
                }
            }
            else
            {
                for (std::size_t i = 0;
                    i < vertices.size();
                    ++i)
                {
                    indices.push_back(
                        static_cast<std::uint32_t>(i)
                    );
                }
            }

            if (vertices.empty() || indices.empty())
                continue;

            Part part;

            part.mesh = std::make_unique<Mesh>(
                vertices, indices
            );

            part.transform = transform;

            part.nodeIndex = static_cast<int>(n);

            part.skinIndex = node.skin
                ? static_cast<int>(node.skin - data->skins)
                : -1;


            // ===== 얼굴 Morph Target 로딩 =====
            // 현재 test.vrm 전용 매핑
            constexpr int FaceNode = 112;
            constexpr int BlinkTarget = 13;
            constexpr int MouthTarget = 39;

            if (static_cast<int>(n) == FaceNode &&
                primitive.targets_count > MouthTarget)
            {
                // Morph 적용 전 원본 정점 저장
                part.baseVertices = vertices;

                // 지정한 Morph Target의 위치 변화량 읽기
                auto readMorphPositions =
                    [&](int targetIndex,
                        std::vector<glm::vec3>& output)
                    {
                        const cgltf_morph_target& target =
                            primitive.targets[targetIndex];

                        const cgltf_accessor* accessor = nullptr;

                        for (cgltf_size a = 0;
                            a < target.attributes_count; ++a)
                        {
                            const cgltf_attribute& attribute =
                                target.attributes[a];

                            if (attribute.type ==
                                cgltf_attribute_type_position)
                            {
                                accessor = attribute.data;
                                break;
                            }
                        }

                        // POSITION 변화량이 없는 Morph는 건너뜀
                        if (!accessor)
                            return;

                        if (accessor->count != vertices.size())
                        {
                            throw std::runtime_error(
                                "Morph vertex count mismatch"
                            );
                        }

                        output.resize(vertices.size());

                        for (cgltf_size i = 0;
                            i < accessor->count; ++i)
                        {
                            float delta[3]{};

                            if (!cgltf_accessor_read_float(
                                accessor, i, delta, 3))
                            {
                                throw std::runtime_error(
                                    "Failed to read morph positions"
                                );
                            }

                            output[i] = glm::vec3(
                                delta[0],
                                delta[1],
                                delta[2]
                            );
                        }
                    };

                readMorphPositions(
                    BlinkTarget,
                    part.blinkDeltas
                );

                readMorphPositions(
                    MouthTarget,
                    part.mouthDeltas
                );
            }


            // 머터리얼 정보
            if (primitive.material)
            {
                const cgltf_material* material =
                    primitive.material;

                if (material->has_pbr_metallic_roughness)
                {
                    const auto& pbr =
                        material->pbr_metallic_roughness;

                    part.baseColor = glm::vec4(
                        pbr.base_color_factor[0],
                        pbr.base_color_factor[1],
                        pbr.base_color_factor[2],
                        pbr.base_color_factor[3]
                    );

                    const cgltf_texture* texture =
                        pbr.base_color_texture.texture;

                    if (texture && texture->image)
                    {
                        const cgltf_image* image =
                            texture->image;

                        auto found = textureCache.find(image);

                        if (found != textureCache.end())
                        {
                            part.texture = found->second;
                        }
                        else
                        {
                            part.texture = loadImage(
                                image, path
                            );

                            textureCache[image] =
                                part.texture;
                        }
                    }
                }
            }

            parts.push_back(std::move(part));
        }
    }

    if (parts.empty())
        throw std::runtime_error(
            "No supported meshes found"
        );

    std::cout << "glTF loaded: "
        << parts.size()
        << " mesh parts\n";


    // GPU 스키닝 행렬용 UBO
    glGenBuffers(1, &skinUBO);

    glBindBuffer(GL_UNIFORM_BUFFER, skinUBO);

    glBufferData(
        GL_UNIFORM_BUFFER,
        Skeleton::MaxJoints * sizeof(glm::mat4),
        nullptr,
        GL_DYNAMIC_DRAW
    );

    glBindBuffer(GL_UNIFORM_BUFFER, 0);

}

void GltfModel::draw(
    const Shader& shader,
    const glm::mat4& modelMatrix
) const
{
    GLuint blockIndex = glGetUniformBlockIndex(
        shader.ID, "SkinMatrices"
    );

    if (blockIndex == GL_INVALID_INDEX)
        throw std::runtime_error("SkinMatrices uniform block not found");

    glUniformBlockBinding(shader.ID, blockIndex, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, skinUBO);

    for (const Part& part : parts)
    {
        const glm::mat4& nodeWorld =
            skeleton->worldTransform(part.nodeIndex);

        shader.setMat4("model", modelMatrix * nodeWorld);

        if (part.skinIndex >= 0)
        {
            Skeleton::Palette palette;
            skeleton->buildPalette(
                part.skinIndex, part.nodeIndex, palette
            );

            glBindBuffer(GL_UNIFORM_BUFFER, skinUBO);
            glBufferSubData(
                GL_UNIFORM_BUFFER, 0,
                sizeof(palette), palette.data()
            );
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
            shader.setInt("useSkinning", 1);
        }
        else
        {
            shader.setInt("useSkinning", 0);
        }

        shader.setVec4("baseColorFactor", part.baseColor);
        shader.setInt("baseTexture", 0);
        glActiveTexture(GL_TEXTURE0);

        if (part.texture)
        {
            shader.setInt("hasTexture", 1);
            part.texture->bind();
        }
        else
        {
            shader.setInt("hasTexture", 0);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        part.mesh->draw();
    }

    glBindTexture(GL_TEXTURE_2D, 0);
}

Skeleton& GltfModel::getSkeleton()
{
    return *skeleton;
}

GltfModel::~GltfModel()
{
    if (skinUBO != 0)
        glDeleteBuffers(1, &skinUBO);
}


void GltfModel::updateFace(
    float blinkWeight,
    float mouthWeight)
{
    blinkWeight = glm::clamp(
        blinkWeight, 0.0f, 1.0f
    );

    mouthWeight = glm::clamp(
        mouthWeight, 0.0f, 1.0f
    );

    for (Part& part : parts)
    {
        // 얼굴 Morph 데이터가 없는 메시 제외
        if (part.baseVertices.empty())
            continue;

        // 항상 원본에서 계산하여 변형 누적 방지
        std::vector<Vertex> deformed =
            part.baseVertices;

        for (std::size_t i = 0;
            i < deformed.size(); ++i)
        {
            glm::vec3 delta(0.0f);

            if (i < part.blinkDeltas.size())
            {
                delta +=
                    part.blinkDeltas[i] * blinkWeight;
            }

            if (i < part.mouthDeltas.size())
            {
                delta +=
                    part.mouthDeltas[i] * mouthWeight;
            }

            deformed[i].position += delta;
        }

        // 변경된 정점을 GPU에 전달
        part.mesh->updateVertices(deformed);
    }
}
