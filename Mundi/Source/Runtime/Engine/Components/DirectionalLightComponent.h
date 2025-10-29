#pragma once
#include "LightComponent.h"
#include "LightManager.h"

// 방향성 라이트 (태양광 같은 평행광)
class UDirectionalLightComponent : public ULightComponent
{
public:
	DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)
	GENERATED_REFLECTION_BODY()

		UDirectionalLightComponent();
	virtual ~UDirectionalLightComponent() override;

public:
	void GetShadowRenderRequests(FSceneView* View, TArray<FShadowRenderRequest>& OutRequests) override;

	// 월드 회전을 반영한 라이트 방향 반환 (Transform의 Forward 벡터)
	FVector GetLightDirection() const;

	// Light Info
	FDirectionalLightInfo GetLightInfo() const;

	// Virtual Interface
	void OnRegister(UWorld* InWorld) override;
	void OnUnregister() override;
	virtual void UpdateLightData() override;
	void OnTransformUpdated() override;

	// Serialization & Duplication
	virtual void OnSerialized() override;
	virtual void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(UDirectionalLightComponent)

	// Update Gizmo to match light properties
	void UpdateDirectionGizmo();

	bool IsOverrideCameraLightPerspective() { return bOverrideCameraLightPerspective; }

protected:
	// Direction Gizmo (shows light direction)
	class UGizmoArrowComponent* DirectionGizmo = nullptr;
	ID3D11ShaderResourceView* ShadowMapSRV = nullptr;
private:
	bool bCascaded = true;
	int CascadedCount = 4;
	float CascadedLinearBlendingValue = 0.5f;
	float CascadedOverlapValue = 0.2f;
	bool bOverrideCameraLightPerspective = false;
	TArray<float> CascadedSliceDepth;

	//로그용
	float CascadedAreaColorDebugValue = 0;
	int CascadedAreaShadowDebugValue = -1;
};
