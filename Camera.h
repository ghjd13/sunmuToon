
#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class Camera
{
public:
    glm::vec3 position{ 2.2f, 1.7f, 3.0f };
    glm::vec3 target{ 0.0f, 0.0f, 0.0f };
    glm::vec3 up{ 0.0f, 1.0f, 0.0f };

    float fov = 45.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;

    glm::mat4 getViewMatrix() const
    {
        return glm::lookAt(
            position,
            target,
            up
        );
    }

    glm::mat4 getProjectionMatrix(
        float aspect
    ) const
    {
        return glm::perspective(
            glm::radians(fov),
            aspect,
            nearPlane,
            farPlane
        );
    }
};
