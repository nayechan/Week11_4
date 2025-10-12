#include "pch.h"
#include "DecalActor.h"
#include "DecalComponent.h"

ADecalActor::ADecalActor()
{
	Name = "Static Mesh Actor";
	DecalComponent = CreateDefaultSubobject<UDecalComponent>("DecalComponent");

	// 루트 교체
	USceneComponent* TempRootComponent = RootComponent;
	RootComponent = DecalComponent;
	RemoveOwnedComponent(TempRootComponent);
}

ADecalActor::~ADecalActor()
{
}

void ADecalActor::DuplicateSubObjects()
{
	Super_t::DuplicateSubObjects();

	// 자식을 순회하면서 UDecalComponent를 찾음
	for (UActorComponent* Component : OwnedComponents)
	{
		if (UDecalComponent* Decal = Cast<UDecalComponent>(Component))
		{
			DecalComponent = Decal;
			break;
		}
	}
}
