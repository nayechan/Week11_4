#include "pch.h"
#include "DirectionalLightComponent.h"

IMPLEMENT_CLASS(UDirectionalLightComponent)

UDirectionalLightComponent::UDirectionalLightComponent()
{
}

UDirectionalLightComponent::~UDirectionalLightComponent()
{
}

FVector UDirectionalLightComponent::GetLightDirection() const
{
	// Z-Up Left-handed 좌표계에서 Forward는 X축
	FQuat Rotation = GetWorldRotation();
	return Rotation.RotateVector(FVector(1.0f, 0.0f, 0.0f));
}

FDirectionalLightInfo UDirectionalLightComponent::GetLightInfo() const
{
	FDirectionalLightInfo Info;
	// Use GetLightColorWithIntensity() to include Temperature + Intensity
	Info.Color = GetLightColorWithIntensity();
	Info.Padding0 = 0.0f;
	Info.Direction = GetLightDirection();
	return Info;
}

void UDirectionalLightComponent::UpdateLightData()
{
	Super::UpdateLightData();
	// 방향성 라이트 특화 업데이트 로직
}

void UDirectionalLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);
	// 추가 속성 없음
}

void UDirectionalLightComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}
