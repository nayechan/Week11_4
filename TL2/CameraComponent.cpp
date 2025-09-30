#include "pch.h"
#include "CameraComponent.h"
#include "FViewport.h"

extern float CLIENTWIDTH;
extern float CLIENTHEIGHT;

UCameraComponent::UCameraComponent()
    : FieldOfView(60.0f)
    , AspectRatio(1.0f / 1.0f)
    , NearClip(0.1f)
    , FarClip(100.0f)
    , ProjectionMode(ECameraProjectionMode::Perspective)
    , ZooMFactor(1.0f)

{
}

UCameraComponent::~UCameraComponent() {}

FMatrix UCameraComponent::GetViewMatrix() const
{
    // Robust path: View = inverse(world) under row-vector convention.
    // Use full transform matrix (translation in last row) and invert affine part.
    const FMatrix World = GetWorldTransform().ToMatrixWithScaleLocalXYZ();
    return World.InverseAffine();
}


FMatrix UCameraComponent::GetProjectionMatrix() const
{
    // 기본 구현은 전체 클라이언트 aspect ratio 사용
    float aspect = CLIENTWIDTH / CLIENTHEIGHT;
    return GetProjectionMatrix(aspect);
}

FMatrix UCameraComponent::GetProjectionMatrix(float ViewportAspectRatio) const
{
    if (ProjectionMode == ECameraProjectionMode::Perspective)
    {
        return FMatrix::PerspectiveFovLH(FieldOfView * (PI / 180.0f),
            ViewportAspectRatio,
            NearClip, FarClip);
    }
    else
    {
        float orthoHeight = 2.0f * tanf((FieldOfView * PI / 180.0f) * 0.5f) * 10.0f;
        float orthoWidth = orthoHeight * ViewportAspectRatio;

        // 줌 계산
        orthoWidth = orthoWidth * ZooMFactor;
        orthoHeight = orthoWidth / ViewportAspectRatio;


        return FMatrix::OrthoLH(orthoWidth, orthoHeight,
            NearClip, FarClip);
    }
}
FMatrix UCameraComponent::GetProjectionMatrix(float ViewportAspectRatio, FViewport* Viewport) const
{
    if (ProjectionMode == ECameraProjectionMode::Perspective)
    {
        return FMatrix::PerspectiveFovLH(
            FieldOfView * (PI / 180.0f),
            ViewportAspectRatio,
            NearClip, FarClip);
    }
    else
    {
        // 1 world unit = 100 pixels (예시)
        const float pixelsPerWorldUnit = 10.0f;

        // 뷰포트 크기를 월드 단위로 변환
        float orthoWidth = (Viewport->GetSizeX() / pixelsPerWorldUnit) * ZooMFactor;
        float orthoHeight = (Viewport->GetSizeY() / pixelsPerWorldUnit) * ZooMFactor;

        return FMatrix::OrthoLH(
            orthoWidth,
            orthoHeight,
            NearClip, FarClip);
    }
}
FVector UCameraComponent::GetForward() const
{
    return GetWorldTransform().Rotation.RotateVector(FVector(1, 0, 0)).GetNormalized();
}

FVector UCameraComponent::GetRight() const
{
    return GetWorldTransform().Rotation.RotateVector(FVector(0, 1, 0)).GetNormalized();
}

FVector UCameraComponent::GetUp() const
{
    return GetWorldTransform().Rotation.RotateVector(FVector(0, 0, 1)).GetNormalized();
}
