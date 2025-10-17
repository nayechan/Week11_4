#include "pch.h"
#include "PointLightActor.h"
#include "PointLightComponent.h"

IMPLEMENT_CLASS(APointLightActor)

APointLightActor::APointLightActor()
{
	Name = "Point Light Actor";
	LightComponent = CreateDefaultSubobject<UPointLightComponent>("PointLightComponent");

	USceneComponent* TempRootComponent = RootComponent;
	RootComponent = LightComponent;
	RemoveOwnedComponent(TempRootComponent);
}

APointLightActor::~APointLightActor()
{
}

void APointLightActor::DuplicateSubObjects()
{
	Super_t::DuplicateSubObjects();

	for (UActorComponent* Component : OwnedComponents)
	{
		if (UPointLightComponent* Light = Cast<UPointLightComponent>(Component))
		{
			LightComponent = Light;
			break;
		}
	}
}

void APointLightActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		LightComponent = Cast<UPointLightComponent>(RootComponent);
	}
}
