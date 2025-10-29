#include "pch.h"
#include "CameraComponent.h"
#include "FViewport.h"

extern float CLIENTWIDTH;
extern float CLIENTHEIGHT;

IMPLEMENT_CLASS(UCameraComponent)

BEGIN_PROPERTIES(UCameraComponent)
	MARK_AS_COMPONENT("카메라 컴포넌트", "카메라를 렌더링하는 컴포넌트입니다.")
	ADD_PROPERTY_RANGE(float, FieldOfView, "Camera", 1.0f, 179.0f, true, "시야각 (FOV, Degrees)입니다.")
	ADD_PROPERTY_RANGE(float, AspectRatio, "Camera", 0.1f, 10.0f, true, "화면 비율입니다.")
	ADD_PROPERTY_RANGE(float, NearClip, "Camera", 0.01f, 1000.0f, true, "근거리 클리핑 평면입니다.")
	ADD_PROPERTY_RANGE(float, FarClip, "Camera", 1.0f, 100000.0f, true, "원거리 클리핑 평면입니다.")
	ADD_PROPERTY_RANGE(float, ZooMFactor, "Camera", 0.1f, 10.0f, true, "줌 배율입니다.")
END_PROPERTIES()

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
    // View 행렬을 Y-Up에서 Z-Up으로 변환하기 위한 행렬
    static const FMatrix YUpToZUp(
        0, 1, 0, 0,
        0, 0, 1, 0,
        1, 0, 0, 0,
        0, 0, 0, 1
    );

	const FMatrix World = GetWorldTransform().ToMatrix();

	return (YUpToZUp * World).InverseAffine();
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
        /*return FMatrix::OrthoLH_XForward(orthoWidth, orthoHeight,
            NearClip, FarClip);*/
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
        /*return FMatrix::OrthoLH_XForward(orthoWidth, orthoHeight,
            NearClip, FarClip);*/
    }
}
TArray<FVector> UCameraComponent::GetFrustumVertices(FViewport* Viewport) const
{
    return GetFrustumVerticesCascaded(Viewport, NearClip, FarClip);
}
TArray<FVector> UCameraComponent::GetFrustumVerticesCascaded(FViewport* Viewport, const float Near, const float Far) const
{
    TArray<FVector> Vertices;
    Vertices.reserve(8);
    float NearHalfHeight, NearHalfWidth, FarHalfHeight, FarHalfWidth;
    if (ProjectionMode == ECameraProjectionMode::Perspective)
    {
        const float Tan = tan(DegreesToRadians(FieldOfView) * 0.5f);
        const float Aspect = Viewport->GetAspectRatio();
        NearHalfHeight = Tan * Near;
        NearHalfWidth = NearHalfHeight * Aspect;
        FarHalfHeight = Tan * Far;
        FarHalfWidth = FarHalfHeight * Aspect;
    }
    else
    {
        const float pixelsPerWorldUnit = 10.0f;

        NearHalfHeight = (Viewport->GetSizeY() / pixelsPerWorldUnit) * ZooMFactor;
        NearHalfWidth = (Viewport->GetSizeX() / pixelsPerWorldUnit) * ZooMFactor;
        FarHalfHeight = NearHalfHeight;
        FarHalfWidth = NearHalfWidth;
    }
    Vertices.emplace_back(FVector(-NearHalfWidth, -NearHalfHeight, Near));
    Vertices.emplace_back(FVector(NearHalfWidth, -NearHalfHeight, Near));
    Vertices.emplace_back(FVector(NearHalfWidth, NearHalfHeight, Near));
    Vertices.emplace_back(FVector(-NearHalfWidth, NearHalfHeight, Near));
    Vertices.emplace_back(FVector(-FarHalfWidth, -FarHalfHeight, Far));
    Vertices.emplace_back(FVector(FarHalfWidth, -FarHalfHeight, Far));
    Vertices.emplace_back(FVector(FarHalfWidth, FarHalfHeight, Far));
    Vertices.emplace_back(FVector(-FarHalfWidth, FarHalfHeight, Far));

    return Vertices;
}
TArray<float> UCameraComponent::GetCascadedSliceDepth(int CascadedCount, float LinearBlending) const
{
    TArray<float> CascadedSliceDepth;
    CascadedSliceDepth.reserve(CascadedCount + 1);
    LinearBlending = LinearBlending < 0 ? 0 : (LinearBlending > 1 ? 1 : LinearBlending); //clamp(0,1,LinearBlending);
    for (int i = 0; i < CascadedCount + 1; i++)
    {
        float CurDepth = 0;
        float LogValue = NearClip * pow((FarClip / NearClip), (float)i / CascadedCount);
        float LinearValue = NearClip + (FarClip - NearClip) * (float)i / CascadedCount;
        CascadedSliceDepth.push_back((1 - LinearBlending) * LogValue + LinearBlending * LinearValue);
    }
    return CascadedSliceDepth;
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

void UCameraComponent::DuplicateSubObjects()
{
    Super::DuplicateSubObjects();

    ProjectionMode = ECameraProjectionMode::Perspective;
}

void UCameraComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);
    // ProjectionMode는 수동 직렬화 (Enum 타입)
    if (bInIsLoading)
    {
        int32 ModeInt = static_cast<int32>(ECameraProjectionMode::Perspective);
        FJsonSerializer::ReadInt32(InOutHandle, "ProjectionMode", ModeInt, 0);
        ProjectionMode = static_cast<ECameraProjectionMode>(ModeInt);
    }
    else
    {
        InOutHandle["ProjectionMode"] = static_cast<int32>(ProjectionMode);
    }
}

void UCameraComponent::OnSerialized()
{
    Super::OnSerialized();

  
}
