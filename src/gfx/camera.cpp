#include "pch.h"
#include "camera.h"

namespace gfx {

    Camera::Camera() {
        mTargetPosition = mPosition = glm::vec3(0.0f, 1.0f, -3.0f);
        mTargetRotation = mRotation = glm::vec3(0.0f, 0.0f, 0.0f);

        mFov = glm::radians(60.0f);
        mAspect = 4.0f / 3.0f;
        mNearPlane = 0.5f;
        mFarPlane = 200.0f;

        mSpeed = 2.0f;
        mSensitivity = 0.8f;

        mIsMoving = false;

        CalculateProjection();
        CalculateView();
    }

    void Camera::Update(float dt) {
        glm::vec3 delta = mTargetPosition - mPosition;
        mPosition += delta * 0.8f * dt;

        glm::vec3 deltaRotation = (mTargetRotation - mRotation);
        mRotation += deltaRotation * 0.8f * dt;

        const float threshold = 0.005f;
        mIsMoving = (glm::length(delta) > threshold) || (glm::length(deltaRotation) > threshold);

        CalculateView();
        mVP = mProjection * mView;

        glm::mat4 vpT = glm::transpose(mVP);
        // Left
        frustumPlanes[0] = vpT[3] + vpT[0];
        // Right
        frustumPlanes[1] = vpT[3] - vpT[0];
        // Top
        frustumPlanes[2] = vpT[3] - vpT[1];
        // Bottom
        frustumPlanes[3] = vpT[3] + vpT[1];
        // Near
        frustumPlanes[4] = vpT[3] + vpT[2];
        // Far
        frustumPlanes[5] = vpT[3] - vpT[2];
        /*
        for (int i = 0; i < frustumPlanes.size(); ++i) {
            float len = glm::length(glm::vec3(frustumPlanes[i]));
            frustumPlanes[i] /= len;
        }
        */
    }

    glm::vec4 Camera::ComputeNDCCoordinate(const glm::vec3 &p) const {
        // Calculate NDC Coordinate
        glm::vec4 ndc = mProjection * mView * glm::vec4(p, 1.0f);
        ndc /= ndc.w;

        // Convert in range 0-1
        ndc = ndc * 0.5f + 0.5f;
        // Invert Y-Axis
        ndc.y = 1.0f - ndc.y;
        return ndc;
    }

    glm::vec4 Camera::ComputeViewSpaceCoordinate(const glm::vec3 &p) const {
        return mView * glm::vec4(p, 1.0f);
    }

    void Camera::GenerateCameraRay(Ray *ray, const glm::vec2 &normalizedMouseCoord) const {
        glm::vec3 clipPos = mInvProjection * glm::vec4(normalizedMouseCoord, -1.0f, 0.0f);
        glm::vec4 worldPos = mInvView * glm::vec4(clipPos.x, clipPos.y, -1.0f, 0.0f);

        ray->direction = glm::normalize(glm::vec3(worldPos));
        ray->origin = mPosition;
    }

    void Camera::CalculateProjection() {
        mProjection = glm::perspective(mFov, mAspect, mNearPlane, mFarPlane);
        mInvProjection = glm::inverse(mProjection);

        mVP = mProjection * mView;
    }

    void Camera::CalculateView() {
        glm::mat3 rotation = glm::yawPitchRoll(mRotation.y, mRotation.x, mRotation.z);

        mForward = glm::normalize(rotation * glm::vec3(0.0f, 0.0f, -1.0f));
        mUp = glm::normalize(rotation * glm::vec3(0.0f, 1.0f, 0.0f));

        mView = glm::lookAt(mPosition, mPosition + mForward, mUp);
        mInvView = glm::inverse(mView);

        mRight = glm::vec3(mView[0][0], mView[1][0], mView[2][0]);
        mUp = glm::vec3(mView[0][1], mView[1][1], mView[2][1]);
        mForward = glm::vec3(mView[0][2], mView[1][2], mView[2][2]);
    }
} // namespace gfx