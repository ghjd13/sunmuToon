
#define _CRT_SECURE_NO_WARNINGS
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include "GltfModel.h"

#include <glm/gtc/type_ptr.hpp>

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
}

void GltfModel::draw(
    const Shader& shader,
    const glm::mat4& modelMatrix
) const
{
    for (const Part& part : parts)
    {
        shader.setMat4(
            "model",
            modelMatrix * part.transform
        );

        shader.setVec4(
            "baseColorFactor",
            part.baseColor
        );

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
