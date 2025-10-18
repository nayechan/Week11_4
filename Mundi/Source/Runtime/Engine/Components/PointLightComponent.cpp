#include "pch.h"
#include "PointLightComponent.h"

IMPLEMENT_CLASS(UPointLightComponent)

UPointLightComponent::UPointLightComponent()
{
	SourceRadius = 0.0f;
}

UPointLightComponent::~UPointLightComponent()
{
}

FPointLightInfo UPointLightComponent::GetLightInfo() const
{
	FPointLightInfo Info;
	// Use GetLightColorWithIntensity() to include Temperature + Intensity
	Info.Color = GetLightColorWithIntensity();
	Info.Position = GetWorldLocation();
	Info.AttenuationRadius = GetAttenuationRadius();  // Moved up for optimal packing
	Info.Attenuation = IsUsingAttenuationCoefficients() ? GetAttenuation() : FVector(1.0f, 0.0f, 0.0f);
	Info.FalloffExponent = IsUsingAttenuationCoefficients() ? 0.0f : GetFalloffExponent();
	Info.bUseAttenuationCoefficients = IsUsingAttenuationCoefficients() ? 1u : 0u;
	Info.Padding = FVector(0.0f, 0.0f, 0.0f); // 패딩 초기화

	return Info;
}

void UPointLightComponent::UpdateLightData()
{
	Super::UpdateLightData();
	// 점광원 특화 업데이트 로직
}

void UPointLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		FJsonSerializer::ReadFloat(InOutHandle, "SourceRadius", SourceRadius, 0.0f);
	}
	else
	{
		InOutHandle["SourceRadius"] = SourceRadius;
	}
}

void UPointLightComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}
