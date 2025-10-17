#include "pch.h"
#include "SpotLightActor.h"
#include "SpotLightComponent.h"

IMPLEMENT_CLASS(ASpotLightActor)

ASpotLightActor::ASpotLightActor()
{
	Name = "Spot Light Actor";
	LightComponent = CreateDefaultSubobject<USpotLightComponent>("SpotLightComponent");

	USceneComponent* TempRootComponent = RootComponent;
	RootComponent = LightComponent;
	RemoveOwnedComponent(TempRootComponent);
}

ASpotLightActor::~ASpotLightActor()
{
}

void ASpotLightActor::DuplicateSubObjects()
{
	Super_t::DuplicateSubObjects();

	for (UActorComponent* Component : OwnedComponents)
	{
		if (USpotLightComponent* Light = Cast<USpotLightComponent>(Component))
		{
			LightComponent = Light;
			break;
		}
	}
}

void ASpotLightActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		LightComponent = Cast<USpotLightComponent>(RootComponent);
	}
}
