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


    // ===== VRM 1.0 Expression 바인딩 읽기 =====

    struct MorphBinding
    {
        int node = -1;
        int index = -1;
        float weight = 1.0f;
    };

    std::unordered_map<
        std::string,
        std::vector<MorphBinding>
    > expressionBindings;

    for (cgltf_size e = 0;
        e < data->data_extensions_count; ++e)
    {
        const cgltf_extension& extension =
            data->data_extensions[e];

        if (!extension.name || !extension.data)
            continue;

        if (std::strcmp(extension.name, "VRMC_vrm") != 0)
            continue;

        auto root = nlohmann::json::parse(
            extension.data, nullptr, false
        );

        if (!root.is_object())
            throw std::runtime_error(
                "Invalid VRM 1.0 extension JSON"
            );

        auto expressions = root.find("expressions");

        if (expressions == root.end() ||
            !expressions->is_object())
            break;

        auto preset = expressions->find("preset");

        if (preset == expressions->end() ||
            !preset->is_object())
            break;

        // 우선 사용할 세 가지 표정
        for (const char* name : { "blink", "aa", "happy" })
        {
            auto expression = preset->find(name);

            if (expression == preset->end() ||
                !expression->is_object())
                continue;

            auto binds =
                expression->find("morphTargetBinds");

            if (binds == expression->end() ||
                !binds->is_array())
                continue;

            for (const auto& bind : *binds)
            {
                if (!bind.is_object())
                    continue;

                auto node = bind.find("node");
                auto index = bind.find("index");

                if (node == bind.end() ||
                    index == bind.end() ||
                    !node->is_number_integer() ||
                    !index->is_number_integer())
                    continue;

                const auto nodeValue =
                    node->get<std::int64_t>();

                const auto targetValue =
                    index->get<std::int64_t>();

                if (nodeValue < 0 ||
                    static_cast<std::uint64_t>(nodeValue) >=
                    data->nodes_count ||
                    targetValue < 0 ||
                    targetValue > 2147483647)
                    continue;

                float weight = 1.0f;

                auto weightField = bind.find("weight");

                if (weightField != bind.end() &&
                    weightField->is_number())
                {
                    weight = weightField->get<float>();
                }

                expressionBindings[name].push_back({
                    static_cast<int>(nodeValue),
                    static_cast<int>(targetValue),
                    glm::clamp(weight, 0.0f, 1.0f)
                    });
            }
        }

        break;
    }

    // 실제로 읽힌 매핑 개수 확인
    for (const char* name : { "blink", "aa", "happy" })
    {
        std::cout
            << "Expression " << name
            << " bindings: "
            << expressionBindings[name].size()
            << '\n';
    }



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

            // ===== glTF 머터리얼 알파 모드 =====
            if (primitive.material)
            {
                const cgltf_material* material =
                    primitive.material;

                switch (material->alpha_mode)
                {
                case cgltf_alpha_mode_mask:
                    part.alphaMode = Part::AlphaMode::Mask;
                    break;

                case cgltf_alpha_mode_blend:
                    part.alphaMode = Part::AlphaMode::Blend;
                    break;

                default:
                    part.alphaMode = Part::AlphaMode::Opaque;
                    break;
                }

                part.alphaCutoff = material->alpha_cutoff;
            }



            // ===== VRM Expression 기반 Morph Target 로딩 =====

            auto accumulateMorphDeltas =
                [&](const char* expressionName,
                    std::vector<glm::vec3>& output)
                {
                    auto found =
                        expressionBindings.find(expressionName);

                    if (found == expressionBindings.end())
                        return;

                    for (const MorphBinding& binding :
                        found->second)
                    {
                        // 해당 표정이 적용되는 노드만 처리
                        if (binding.node != static_cast<int>(n))
                            continue;

                        if (binding.index < 0 ||
                            static_cast<cgltf_size>(binding.index) >=
                            primitive.targets_count)
                        {
                            std::cerr
                                << "Invalid morph target: "
                                << expressionName << '\n';
                            continue;
                        }

                        const cgltf_morph_target& target =
                            primitive.targets[binding.index];

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

                        if (!accessor)
                            continue;

                        if (accessor->count != vertices.size())
                            throw std::runtime_error(
                                "Morph vertex count mismatch"
                            );

                        if (output.empty())
                        {
                            output.assign(
                                vertices.size(),
                                glm::vec3(0.0f)
                            );
                        }

                        for (cgltf_size i = 0;
                            i < accessor->count; ++i)
                        {
                            float delta[3]{};

                            if (!cgltf_accessor_read_float(
                                accessor, i, delta, 3))
                            {
                                throw std::runtime_error(
                                    "Failed to read morph delta"
                                );
                            }

                            // 바인딩 가중치를 포함하여 누적
                            output[i] += glm::vec3(
                                delta[0],
                                delta[1],
                                delta[2]
                            ) * binding.weight;
                        }
                    }
                };

            accumulateMorphDeltas(
                "blink", part.blinkDeltas
            );

            accumulateMorphDeltas(
                "aa", part.mouthDeltas
            );

            accumulateMorphDeltas(
                "happy", part.happyDeltas
            );

            // Morph가 존재하는 Part만 원본 정점 보관
            if (!part.blinkDeltas.empty() ||
                !part.mouthDeltas.empty() ||
                !part.happyDeltas.empty())
            {
                part.baseVertices = vertices;
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

            // ===== glTF Normal Map =====
            if (primitive.material)
            {
                const cgltf_material* material =
                    primitive.material;

            const cgltf_texture* normalSource =
                material->normal_texture.texture;

            if (normalSource && normalSource->image)
            {
                // 현재 Vertex에는 UV 채널 하나만 존재
                if (uv &&
                    material->normal_texture.texcoord == uvSet)
                {
                    const cgltf_image* image =
                        normalSource->image;

                    auto found = textureCache.find(image);

                    if (found != textureCache.end())
                    {
                        part.normalTexture = found->second;
                    }
                    else
                    {
                        part.normalTexture = loadImage(
                            image, path
                        );

                        textureCache[image] =
                            part.normalTexture;
                    }

                    part.normalScale =
                        material->normal_texture.scale;
                }
                else
                {
                    std::cerr
                        << "[Normal Map] Unsupported UV set: "
                        << (material->name
                            ? material->name : "Unnamed")
                        << '\n';}
                }
            }

            // ===== VRM 1.0 MToon 파라미터 =====
            if (primitive.material)
            {
                const cgltf_material* material =
                    primitive.material;

                for (cgltf_size e = 0;
                    e < material->extensions_count; ++e)
                {
                    const cgltf_extension& extension =
                        material->extensions[e];

                    if (!extension.name || !extension.data)
                        continue;

                    if (std::strcmp(
                        extension.name,
                        "VRMC_materials_mtoon") != 0)
                    {
                        continue;
                    }

                    auto mtoon = nlohmann::json::parse(
                        extension.data, nullptr, false
                    );

                    if (!mtoon.is_object())
                    {
                        std::cerr
                            << "Invalid MToon JSON\n";
                        break;
                    }

                    part.hasMToon = true;


                    // BLEND일 때 깊이 버퍼에 기록할지 여부
                    auto zwrite = mtoon.find("transparentWithZWrite");

                    if (zwrite != mtoon.end() &&
                        zwrite->is_boolean())
                    {
                        part.transparentWithZWrite =
                            zwrite->get<bool>();
                    }

                    // MToon 투명 머터리얼의 렌더링 순서
                    auto queue = mtoon.find("renderQueueOffsetNumber");

                    if (queue != mtoon.end() &&
                        queue->is_number_integer())
                    {
                        part.renderQueueOffset =
                            queue->get<int>();
                    }


                    // 1. 그림자색
                    auto shade =
                        mtoon.find("shadeColorFactor");

                    if (shade != mtoon.end() &&
                        shade->is_array() &&
                        shade->size() == 3 &&
                        (*shade)[0].is_number() &&
                        (*shade)[1].is_number() &&
                        (*shade)[2].is_number())
                    {
                        part.mtoonShadeColor = glm::vec4(
                            (*shade)[0].get<float>(),
                            (*shade)[1].get<float>(),
                            (*shade)[2].get<float>(),
                            1.0f
                        );
                    }

                    // 2. 명암 경계 이동
                    auto shift =
                        mtoon.find("shadingShiftFactor");

                    if (shift != mtoon.end() &&
                        shift->is_number())
                    {
                        part.mtoonShadingShift =
                            shift->get<float>();
                    }

                    // 3. 툰 명암 선명도
                    auto toony =
                        mtoon.find("shadingToonyFactor");

                    if (toony != mtoon.end() &&
                        toony->is_number())
                    {
                        part.mtoonShadingToony =
                            glm::clamp(
                                toony->get<float>(),
                                0.0f,
                                1.0f
                            );
                    }

                    std::cout
                        << "[MToon] "
                        << (material->name
                            ? material->name
                            : "Unnamed")
                        << " | Shade: "
                        << part.mtoonShadeColor.r << ", "
                        << part.mtoonShadeColor.g << ", "
                        << part.mtoonShadeColor.b
                        << " | Shift: "
                        << part.mtoonShadingShift
                        << " | Toony: "
                        << part.mtoonShadingToony
                        << '\n';


                    // ===== MToon 그림자 텍스처 =====

                    auto shadeInfo = mtoon.find("shadeMultiplyTexture");

                    if (shadeInfo != mtoon.end() &&
                        shadeInfo->is_object())
                    {
                        auto indexField = shadeInfo->find("index");

                        if (indexField != shadeInfo->end() &&
                            indexField->is_number_integer())
                        {
                            const std::int64_t textureIndex =
                                indexField->get<std::int64_t>();

                            // 현재 Vertex는 UV 채널 하나만 보관하므로
                            // 기본 텍스처와 UV 채널이 같을 때만 지원
                            int shadeUvSet = 0;

                            auto uvField = shadeInfo->find("texCoord");

                            if (uvField != shadeInfo->end() &&
                                uvField->is_number_integer())
                            {
                                shadeUvSet = uvField->get<int>();
                            }

                            if (shadeUvSet != uvSet)
                            {
                                std::cerr
                                    << "Shade texture UV set unsupported: "
                                    << shadeUvSet << '\n';
                            }
                            else if (textureIndex >= 0 &&
                                static_cast<std::uint64_t>(
                                    textureIndex) < data->textures_count)
                            {
                                const cgltf_texture& shadeTexture =
                                    data->textures[textureIndex];

                                const cgltf_image* image =
                                    shadeTexture.image;

                                if (image)
                                {
                                    auto found = textureCache.find(image);

                                    if (found != textureCache.end())
                                    {
                                        // 이미 로드된 텍스처 재사용
                                        part.shadeTexture = found->second;
                                    }
                                    else
                                    {
                                        part.shadeTexture =
                                            loadImage(image, path);

                                        textureCache[image] =
                                            part.shadeTexture;
                                    }
                                }
                            }
                        }
                    }

                    std::cout
                        << "  Shade texture: "
                        << (part.shadeTexture ? "Loaded" : "None")
                        << '\n';


                    break;
                }
            }
            // ===== Normal Map / Tangent 진단 =====
            if (primitive.material)
            {
                const cgltf_material* material =
                    primitive.material;

                const bool hasNormalMap =
                    material->normal_texture.texture != nullptr;

                const cgltf_accessor* tangent =
                    cgltf_find_accessor(
                        &primitive,
                        cgltf_attribute_type_tangent,
                        0
                    );

                std::cout
                    << "[Normal Map] "
                    << (material->name
                        ? material->name : "Unnamed")
                    << " | Texture: "
                    << (hasNormalMap ? "Yes" : "No")
                    << " | Tangent: "
                    << (tangent ? "Yes" : "No")
                    << '\n';
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

    std::size_t mtoonPartCount = 0;

    for (const Part& part : parts)
    {
        if (part.hasMToon)
            ++mtoonPartCount;
    }

    std::cout << "MToon-enabled parts: "
        << mtoonPartCount
        << " / " << parts.size() << '\n';

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
    // GPU 스키닝 UBO 연결
    GLuint blockIndex = glGetUniformBlockIndex(
        shader.ID, "SkinMatrices"
    );

    if (blockIndex == GL_INVALID_INDEX)
        throw std::runtime_error(
            "SkinMatrices uniform block not found"
        );

    glUniformBlockBinding(shader.ID, blockIndex, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, skinUBO);

    // ===== 1. 머터리얼별 렌더링 목록 구성 =====

    std::vector<const Part*> opaqueParts;
    std::vector<const Part*> maskParts;
    std::vector<const Part*> blendZWriteParts;
    std::vector<const Part*> blendParts;

    for (const Part& part : parts)
    {
        switch (part.alphaMode)
        {
        case Part::AlphaMode::Opaque:
            opaqueParts.push_back(&part);
            break;

        case Part::AlphaMode::Mask:
            maskParts.push_back(&part);
            break;

        case Part::AlphaMode::Blend:
            if (part.transparentWithZWrite)
                blendZWriteParts.push_back(&part);
            else
                blendParts.push_back(&part);
            break;
        }
    }

    // MToon Render Queue Offset 오름차순 정렬
    auto sortByQueue = [](const Part* a, const Part* b)
        {
            return a->renderQueueOffset <
                b->renderQueueOffset;
        };

    std::stable_sort(
        blendZWriteParts.begin(),
        blendZWriteParts.end(),
        sortByQueue
    );

    std::stable_sort(
        blendParts.begin(),
        blendParts.end(),
        sortByQueue
    );

    // ===== 2. 메시 한 개 렌더링 =====

    auto renderPart = [&](const Part& part)
        {
            const glm::mat4& nodeWorld =
                skeleton->worldTransform(part.nodeIndex);

            shader.setMat4(
                "model",
                modelMatrix * nodeWorld
            );

            // 기존 GPU 스키닝
            if (part.skinIndex >= 0)
            {
                Skeleton::Palette palette;

                skeleton->buildPalette(
                    part.skinIndex,
                    part.nodeIndex,
                    palette
                );

                glBindBuffer(GL_UNIFORM_BUFFER, skinUBO);

                glBufferSubData(
                    GL_UNIFORM_BUFFER,
                    0,
                    sizeof(palette),
                    palette.data()
                );

                glBindBuffer(GL_UNIFORM_BUFFER, 0);

                shader.setInt("useSkinning", 1);
            }
            else
            {
                shader.setInt("useSkinning", 0);
            }

            // 기존 기본 머터리얼
            shader.setVec4(
                "baseColorFactor",
                part.baseColor
            );

            // 기존 MToon 파라미터
            shader.setInt(
                "hasMToon",
                part.hasMToon ? 1 : 0
            );

            shader.setVec4(
                "mtoonShadeColor",
                part.mtoonShadeColor
            );

            shader.setFloat(
                "mtoonShadingShift",
                part.mtoonShadingShift
            );

            shader.setFloat(
                "mtoonShadingToony",
                part.mtoonShadingToony
            );

            // 새 알파 모드
            shader.setInt(
                "alphaMode",
                static_cast<int>(part.alphaMode)
            );

            shader.setFloat(
                "alphaCutoff",
                part.alphaCutoff
            );

            // 기존 텍스처
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

            // ===== MToon 그림자 텍스처 =====

// 기본 텍스처는 GL_TEXTURE0 사용 중
// 그림자 텍스처는 GL_TEXTURE1 사용
            shader.setInt("shadeTexture", 1);

            glActiveTexture(GL_TEXTURE1);

            if (part.shadeTexture)
            {
                shader.setInt("hasShadeTexture", 1);
                part.shadeTexture->bind();
            }
            else
            {
                shader.setInt("hasShadeTexture", 0);
                glBindTexture(GL_TEXTURE_2D, 0);
            }

            // 활성 텍스처 유닛을 기존 0번으로 복구
            glActiveTexture(GL_TEXTURE0);


            // ===== Normal Map 바인딩 =====

            // 0: 기본 텍스처
            // 1: MToon 그림자 텍스처
            // 2: 노멀맵
            shader.setInt("normalMapTexture", 2);

            shader.setFloat(
                "normalScale",
                part.normalScale
            );

            glActiveTexture(GL_TEXTURE2);

            if (part.normalTexture)
            {
                shader.setInt("hasNormalMap", 1);
                part.normalTexture->bind();
            }
            else
            {
                shader.setInt("hasNormalMap", 0);
                glBindTexture(GL_TEXTURE_2D, 0);
            }

            // 노멀맵을 유지한 상태에서 렌더링
            glActiveTexture(GL_TEXTURE0);

            part.mesh->draw();
        };

    // ===== 3. OPAQUE / MASK =====
    // 불투명 파트를 먼저 그려 깊이 버퍼 채우기

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);

    for (const Part* part : opaqueParts)
        renderPart(*part);

    for (const Part* part : maskParts)
        renderPart(*part);

    // ===== 4. BLEND =====
    // 반투명 픽셀을 배경과 혼합

    glEnable(GL_BLEND);

    glBlendFuncSeparate(
        GL_SRC_ALPHA,
        GL_ONE_MINUS_SRC_ALPHA,
        GL_ONE,
        GL_ONE_MINUS_SRC_ALPHA
    );

    // MToon: 투명하지만 깊이 기록 허용
    glDepthMask(GL_TRUE);

    for (const Part* part : blendZWriteParts)
        renderPart(*part);

    // 일반 반투명: 깊이 테스트는 유지,
    // 깊이 버퍼 기록만 중지
    glDepthMask(GL_FALSE);

    for (const Part* part : blendParts)
        renderPart(*part);

    // ===== 5. OpenGL 상태 복원 =====

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    // 노멀맵 해제
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, 0);

    // 그림자 텍스처 해제
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, 0);

    // 기본 텍스처 유닛 복구
    glActiveTexture(GL_TEXTURE0);
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
    float mouthWeight,
    float happyWeight)
{
    blinkWeight = glm::clamp(
        blinkWeight, 0.0f, 1.0f
    );

    mouthWeight = glm::clamp(
        mouthWeight, 0.0f, 1.0f
    );

    happyWeight = glm::clamp(
        happyWeight, 0.0f, 1.0f
    );

    // 이전 프레임과 표정이 같다면 VBO 갱신 생략
    if (blinkWeight == lastBlinkWeight &&
        mouthWeight == lastMouthWeight &&
        happyWeight == lastHappyWeight)
    {
        return;
    }

    lastBlinkWeight = blinkWeight;
    lastMouthWeight = mouthWeight;
    lastHappyWeight = happyWeight;

    for (Part& part : parts)
    {
        if (part.baseVertices.empty())
            continue;

        // 항상 원본 정점에서 다시 변형
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

            if (i < part.happyDeltas.size())
            {
                delta +=
                    part.happyDeltas[i] * happyWeight;
            }

            deformed[i].position += delta;
        }

        part.mesh->updateVertices(deformed);
    }
}

