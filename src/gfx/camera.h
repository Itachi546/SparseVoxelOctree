#pragma once

#include "math-utils.h"

#include <array>
#include <memory>

struct Ray;

namespace gfx {

    class Camera {
      public:
        Camera();

        void Update(float dt);
        glm::mat4 GetProjectionMatrix() const { return mProjection; }
        glm::mat4 GetViewMatrix() const { return mView; }

        glm::mat4 GetInvProjectionMatrix() const { return mInvProjection; }
        glm::mat4 GetInvViewMatrix() const { return mInvView; }
        glm::mat4 GetViewProjectionMatrix() const { return mVP; }

        void SetPosition(glm::vec3 p) {
            mPosition = p;
            mTargetPosition = p;
        }

        void SetRotation(glm::vec3 rotation) {
            mRotation = mTargetRotation = rotation;
        }

        glm::vec3 GetRotation() const {
            return mRotation;
        }

        void SetNearPlane(float d) {
            mNearPlane = d;
            CalculateProjection();
        }

        void SetFarPlane(float d) {
            mFarPlane = d;
            CalculateProjection();
        }

        void SetAspect(float aspect) {
            mAspect = aspect;
            CalculateProjection();
        }

        void SetSpeed(float speed) {
            mSpeed = speed;
        }

        void SetSensitivity(float sensitivity) {
            mSensitivity = sensitivity;
        }

        void SetFOV(float fov) {
            mFov = fov;
            CalculateProjection();
        }

        inline float GetFOV() const { return mFov; }
        inline float GetAspect() const { return mAspect; }
        inline float GetNearPlane() const { return mNearPlane; }
        inline float GetFarPlane() const { return mFarPlane; }

        void Walk(float dt) {
            mTargetPosition += mSpeed * dt * mForward;
        }

        void Strafe(float dt) {
            mTargetPosition += mSpeed * dt * mRight;
        }

        void Lift(float dt) {
            mTargetPosition += mSpeed * dt * mUp;
        }

        void Rotate(float dx, float dy, float dt) {
            float m = mSensitivity * dt;
            mTargetRotation.y += dy * m;
            mTargetRotation.x += dx * m;

            constexpr float maxAngle = glm::radians(89.99f);
            mTargetRotation.x = glm::clamp(mTargetRotation.x, -maxAngle, maxAngle);
        }

        bool IsMoving() {
            return mIsMoving;
        }

        glm::vec4 ComputeNDCCoordinate(const glm::vec3 &p) const;
        glm::vec4 ComputeViewSpaceCoordinate(const glm::vec3 &p) const;

        void GenerateCameraRay(Ray *ray, const glm::vec2 &normalizedMouseCoord) const;

        glm::vec3 GetForward() { return mForward; }
        glm::vec3 GetPosition() const { return mPosition; }

        std::array<glm::vec4, 6> frustumPlanes;

      private:
        bool mIsMoving;
        glm::vec3 mPosition;
        glm::vec3 mRotation;

        glm::vec3 mTargetPosition;
        glm::vec3 mTargetRotation;

        glm::mat4 mProjection, mInvProjection;
        glm::mat4 mVP;

        float mFov;
        float mAspect;
        float mNearPlane;
        float mFarPlane;

        glm::mat4 mView, mInvView;
        glm::vec3 mRight;
        glm::vec3 mUp;
        glm::vec3 mForward;
        float mSpeed;
        float mSensitivity;

        void CalculateProjection();
        void CalculateView();
    };
} // namespace gfx
