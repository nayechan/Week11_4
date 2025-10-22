#include "pch.h"
#include "DirectionalLightComponent.h"
#include "BillboardComponent.h"
#include "Gizmo/GizmoArrowComponent.h"

UDirectionalLightComponent::UDirectionalLightComponent()
{
}

UDirectionalLightComponent::~UDirectionalLightComponent()
{
}

IMPLEMENT_CLASS(UDirectionalLightComponent)

BEGIN_PROPERTIES(UDirectionalLightComponent)
	MARK_AS_COMPONENT("디렉셔널 라이트", "방향성 라이트 (태양광 같은 평행광) 컴포넌트입니다.")
END_PROPERTIES()

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
	Info.Direction = GetLightDirection();
	Info.Padding = 0.0f; // 패딩 초기화

	return Info;
}

void UDirectionalLightComponent::OnRegister(UWorld* InWorld)
{
	Super_t::OnRegister(InWorld);
	SpriteComponent->SetTextureName(GDataDir + "/UI/Icons/S_LightDirectional.dds");

	// Create Direction Gizmo if not already created
	if (!DirectionGizmo)
	{
		CREATE_EDITOR_COMPONENT(DirectionGizmo, UGizmoArrowComponent);

		// Set gizmo mesh (using the same mesh as GizmoActor's arrow)
		DirectionGizmo->SetStaticMesh(GDataDir + "/Gizmo/TranslationHandle.obj");
		DirectionGizmo->SetMaterialByName(0, "Shaders/UI/Gizmo.hlsl");

		// Use world-space scale (not screen-constant scale like typical gizmos)
		DirectionGizmo->SetUseScreenConstantScale(false);

		// Set default scale
		DirectionGizmo->SetDefaultScale(FVector(0.5f, 0.3f, 0.3f));

		// Update gizmo properties to match light
		UpdateDirectionGizmo();
	}
	InWorld->GetLightManager()->RegisterLight(this);
}

void UDirectionalLightComponent::OnUnregister()
{
	GWorld->GetLightManager()->DeRegisterLight(this);
}

void UDirectionalLightComponent::UpdateLightData()
{
	Super::UpdateLightData();
	// 방향성 라이트 특화 업데이트 로직
	GWorld->GetLightManager()->UpdateLight(this);
	// Update direction gizmo to reflect any changes
	UpdateDirectionGizmo();
}

void UDirectionalLightComponent::OnTransformUpdated()
{
	Super::OnTransformUpdated();
	GWorld->GetLightManager()->UpdateLight(this);
}

void UDirectionalLightComponent::OnSerialized()
{
	Super::OnSerialized();

}

void UDirectionalLightComponent::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();
}

void UDirectionalLightComponent::UpdateDirectionGizmo()
{
	if (!DirectionGizmo)
		return;

	// Set direction to match light direction (used for hovering/picking, not for rendering)
	FVector LightDir = GetLightDirection();
	DirectionGizmo->SetDirection(LightDir);

	// Set color to match base light color (without intensity or temperature multipliers)
	const FLinearColor& BaseColor = GetLightColor();
	DirectionGizmo->SetColor(FVector(BaseColor.R, BaseColor.G, BaseColor.B));

	// DirectionGizmo is a child of DirectionalLightComponent, so it inherits the parent's rotation automatically.
	// Arrow mesh points along +X axis by default, which matches the DirectionalLight's forward direction.
	// No need to set RelativeRotation - it should remain identity (0, 0, 0)
}
