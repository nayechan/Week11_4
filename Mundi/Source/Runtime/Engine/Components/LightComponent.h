#pragma once
#include "LightComponentBase.h"

class FSceneView;

struct FShadowRenderRequest;


// 실제 조명 효과를 가진 라이트들의 공통 베이스
class ULightComponent : public ULightComponentBase
{
public:
	DECLARE_CLASS(ULightComponent, ULightComponentBase)
	GENERATED_REFLECTION_BODY()

	ULightComponent();
	virtual ~ULightComponent() override;

public:
	// Temperature
	void SetTemperature(float InTemperature) { Temperature = InTemperature;}
	float GetTemperature() const { return Temperature; }

	// 색상과 강도를 합쳐서 반환
	virtual FLinearColor GetLightColorWithIntensity() const;
	void OnRegister(UWorld* InWorld) override;

	virtual void GetShadowRenderRequests(FSceneView* View, TArray<FShadowRenderRequest>& OutRequests) {};

	// Serialization & Duplication
	virtual void OnSerialized() override;
	virtual void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(ULightComponent)

	float GetShadowResolutionScale() { return ShadowResolutionScale; }
	float GetShadowBias() { return ShadowBias; }
	float GetShadowSlopeBias() { return ShadowSlopeBias; }
	float GetShadowSharpen() { return ShadowSharpen; }
	EShadowMapType GetShadowMapType() const { return ShadowMapType; }

protected:
	float Temperature = 6500.0f; // 색온도 (K)

	int ShadowResolutionScale = 1024;
	float ShadowBias = 0.0f;	// NOTE: 추후 필요한 기본값으로 설정 필요
	float ShadowSlopeBias = 0.0f;	// NOTE: 추후 필요한 기본값으로 설정 필요
	float ShadowSharpen = 0.0f;	// NOTE: 추후 필요한 기본값으로 설정 필요

	EShadowMapType ShadowMapType = EShadowMapType::Standard;
};
