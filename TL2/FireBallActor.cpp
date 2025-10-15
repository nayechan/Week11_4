#include "pch.h"
#include "FireBallActor.h"
#include "StaticMeshComponent.h"
#include "WorldPartitionManager.h"

AFireBallActor::AFireBallActor()
{
	Name = "Fire Ball Actor";
	FireBallComponent = CreateDefaultSubobject<UFireBallComponent>("FireBallComponent");
	// 루트 교체
	USceneComponent* TempRootComponent = RootComponent;
	RootComponent = FireBallComponent;
	RemoveOwnedComponent(TempRootComponent);

	// TargetActorTransformWidget.cpp의 TryAttachComponentToActor 메소드 참고해 구체 스태틱메시 컴포넌트 부착하는 코드 구현
	// TryAttachComponentToActor는 익명 네임스페이스에 있어 직접 사용 불가능
	UStaticMeshComponent* FireBallSMC = CreateDefaultSubobject<UStaticMeshComponent>("FireBallStaticMeshComponent");
	FireBallSMC->SetStaticMesh("Data/Model/Sphere.obj");
	FireBallSMC->SetupAttachment(RootComponent, EAttachmentRule::KeepRelative);
}

AFireBallActor::~AFireBallActor()
{
}

void AFireBallActor::DuplicateSubObjects()
{
	Super_t::DuplicateSubObjects();
	// 자식을 순회하면서 UDecalComponent를 찾음
	for (UActorComponent* Component : OwnedComponents)
	{
		if (UFireBallComponent* FireBall = Cast<UFireBallComponent>(Component))
		{
			FireBallComponent = FireBall;
			break;
		}
	}
}

void AFireBallActor::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);
	if (bInIsLoading)
	{
		FireBallComponent = Cast<UFireBallComponent>(RootComponent);
	}
}