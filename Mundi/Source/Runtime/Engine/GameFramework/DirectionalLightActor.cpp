#include "pch.h"
#include "DirectionalLightActor.h"
#include "DirectionalLightComponent.h"

IMPLEMENT_CLASS(ADirectionalLightActor)

ADirectionalLightActor::ADirectionalLightActor()
{
	Name = "Directional Light Actor";
	LightComponent = CreateDefaultSubobject<UDirectionalLightComponent>("DirectionalLightComponent");

	USceneComponent* TempRootComponent = RootComponent;
	RootComponent = LightComponent;
	RemoveOwnedComponent(TempRootComponent);
}

ADirectionalLightActor::~ADirectionalLightActor()
{
}

void ADirectionalLightActor::DuplicateSubObjects()
{
	Super_t::DuplicateSubObjects();

	for (UActorComponent* Component : OwnedComponents)
	{
		if (UDirectionalLightComponent* Light = Cast<UDirectionalLightComponent>(Component))
		{
			LightComponent = Light;
			break;
		}
	}
}

void ADirectionalLightActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		LightComponent = Cast<UDirectionalLightComponent>(RootComponent);
	}
}
