
#define _CRT_SECURE_NO_WARNINGS
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include "GltfModel.h"

#include <glm/gtc/type_ptr.hpp>

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <utility>

GltfModel::GltfModel(const std::string& path)
{
    cgltf_options options{};
    cgltf_data* rawData = nullptr;

    // GLB / glTF 파일 파싱
    cgltf_result result = cgltf_parse_file(
        &options, path.c_str(), &rawData
    );

    // 자동 메모리 해제
    std::unique_ptr<cgltf_data, decltype(&cgltf_free)>
        data(rawData, &cgltf_free);

    if (result != cgltf_result_success) {
        throw std::runtime_error(
            "Failed to parse glTF: " + path
        );
    }

    // 실제 바이너리 버퍼 로딩
    result = cgltf_load_buffers(
        &options, data.get(), path.c_str()
    );

    if (result != cgltf_result_success) {
        throw std::runtime_error(
            "Failed to load glTF buffers"
        );
    }

    // 각 노드에 연결된 메시 처리
    for (cgltf_size n = 0; n < data->nodes_count; ++n)
    {
        const cgltf_node& node = data->nodes[n];

        if (!node.mesh)
            continue;

        // 노드의 월드 변환 행렬
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

            // 삼각형 메시만 처리
            if (primitive.type !=
                cgltf_primitive_type_triangles)
                continue;

            // 정점 위치 accessor 검색
            const cgltf_accessor* positionAccessor =
                cgltf_find_accessor(
                    &primitive,
                    cgltf_attribute_type_position,
                    0
                );

            if (!positionAccessor ||
                positionAccessor->type != cgltf_type_vec3)
                continue;

            std::vector<glm::vec3> positions;
            std::vector<std::uint32_t> indices;

            positions.reserve(positionAccessor->count);

            // 정점 좌표 읽기
            for (cgltf_size i = 0;
                i < positionAccessor->count;
                ++i)
            {
                float position[3]{};

                if (!cgltf_accessor_read_float(
                    positionAccessor, i, position, 3))
                {
                    throw std::runtime_error(
                        "Failed to read vertex position"
                    );
                }

                positions.emplace_back(
                    position[0],
                    position[1],
                    position[2]
                );
            }

            // 인덱스 읽기
            if (primitive.indices)
            {
                const cgltf_accessor* accessor =
                    primitive.indices;

                indices.reserve(accessor->count);

                for (cgltf_size i = 0;
                    i < accessor->count;
                    ++i)
                {
                    cgltf_size index =
                        cgltf_accessor_read_index(
                            accessor, i
                        );

                    if (index >= positions.size()) {
                        throw std::runtime_error(
                            "Invalid glTF index"
                        );
                    }

                    indices.push_back(
                        static_cast<std::uint32_t>(index)
                    );
                }
            }
            else
            {
                // 인덱스가 없는 메시
                for (std::size_t i = 0;
                    i < positions.size();
                    ++i)
                {
                    indices.push_back(
                        static_cast<std::uint32_t>(i)
                    );
                }
            }

            if (positions.empty() || indices.empty())
                continue;

            // GPU 메시 생성
            Part part;
            part.mesh = std::make_unique<Mesh>(
                positions, indices
            );

            part.transform = transform;

            parts.push_back(std::move(part));
        }
    }

    if (parts.empty()) {
        throw std::runtime_error(
            "No supported triangle meshes in model"
        );
    }

    std::cout << "glTF loaded: "
        << parts.size()
        << " mesh parts\n";
}

void GltfModel::draw(
    const Shader& shader,
    const glm::mat4& modelMatrix
) const
{
    for (const Part& part : parts)
    {
        glm::mat4 finalMatrix =
            modelMatrix * part.transform;

        shader.setMat4("model", finalMatrix);

        part.mesh->draw();
    }
}
