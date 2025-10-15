#include "pch.h"
#include "FireBallComponent.h"

UFireBallComponent::UFireBallComponent()
{
	SetCanEverTick(true);
	SetTickEnabled(true);

	LightingShaderPath = "FireBallShader.hlsl";
}

UFireBallComponent::~UFireBallComponent()
{
	ShadowResources = nullptr;
}

#pragma region Lifecycle
void UFireBallComponent::InitializeComponent()
{
	Super::InitializeComponent();
}

void UFireBallComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UFireBallComponent::TickComponent(float DeltaTime)
{
	Super::TickComponent(DeltaTime);
}

void UFireBallComponent::EndPlay(EEndPlayReason Reason)
{
	Super::EndPlay(Reason);
}
#pragma endregion

#pragma region Rendering (WIP)
void UFireBallComponent::Render(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj)
{
}

void UFireBallComponent::RenderDebugVolume(URenderer* Renderer, const FMatrix& View, const FMatrix& Proj) const
{
}
#pragma endregion

#pragma region Parameters
void UFireBallComponent::SetIntensity(float InIntensity)
{
	Intensity = std::max(0.0f, InIntensity);
}

void UFireBallComponent::SetRadius(float InRadius)
{
	Radius = std::max(0.0f, InRadius);
}

void UFireBallComponent::SetRadiusFallOff(float InRadiusFallOff)
{
	RadiusFallOff = std::max(0.0f, InRadiusFallOff);
}

void UFireBallComponent::SetColor(const FLinearColor& InColor)
{
	Color = InColor;
}

FBoundingSphere UFireBallComponent::GetBoundingSphere() const
{
	return FBoundingSphere(GetWorldLocation(), Radius);
}

void UFireBallComponent::SetShadowCaptureEnabled(bool bEnabled)
{
	bShadowCaptureEnabled = bEnabled;
}

void UFireBallComponent::SetShadowMapResolution(uint32 InResolution)
{
	ShadowMapResolution = std::max(128u, InResolution);
}
#pragma endregion

void UFireBallComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	// TODO
}

void UFireBallComponent::DuplicateSubObjects()
{
	// TODO
}

void UFireBallComponent::OnTransformUpdatedChildImpl()
{
	// TODO
}