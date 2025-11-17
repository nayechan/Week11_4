#include "pch.h"
#include "SpringArmComponent.h"
#include "World.h"
#include "WorldPartitionManager.h"
#include "BVHierarchy.h"
#include "Picking.h"
#include "Actor.h"
#include "Renderer.h"
#include "PlayerController.h"


USpringArmComponent::USpringArmComponent()
{
	bCanEverTick = true;
	bTickEnabled = true;
	bTickInEditor = true;
}

void USpringArmComponent::OnRegister(UWorld* InWorld)
{
	Super_t::OnRegister(InWorld);
	if(Owner)
		Owner->SetTickInEditor(true);
	bSocketValid = false;
	bInitLag = false;
}

void USpringArmComponent::BeginPlay()
{
	InitialRotation = GetRelativeRotation();
}

void USpringArmComponent::TickComponent(float DeltaTime)
{
	if (!IsComponentTickEnabled()) return;

	Super_t::TickComponent(DeltaTime);

	APlayerController* PlayerController = GWorld->GetPlayerController();
	if (bUseControllerRotation && PlayerController)
	{
		SetWorldRotation(PlayerController->GetControlRotation());
	}
	EvaluateArm(DeltaTime);
}

FTransform USpringArmComponent::GetSocketWorldTransform() const
{
	if (!bSocketValid)
	{
		// const 함수 내에서 멤버 업데이트 하기 위해 const_cast 사용
		const_cast<USpringArmComponent*>(this)->EvaluateArm(0.0f);
	}
	return CachedSocketWorld;
}

void USpringArmComponent::RenderDebugVolume(URenderer* Renderer) const
{
	const FLinearColor LineColor(1.0f, 0.0f, 0.0f, 1.0f);

	FVector Start = GetWorldTransform().TransformPosition(TargetOffset);
	FVector End = GetSocketWorldTransform().Translation;
	Renderer->AddLine(Start, End, LineColor.ToFVector4());
}

void USpringArmComponent::EvaluateArm(float DeltaTime)
{
	const FTransform WorldTransform = GetWorldTransform();
	const FQuat WorldRotation = WorldTransform.Rotation;
	const FVector WorldLocation = WorldTransform.Translation;

	
	// 피벗 위치: 부모 기준 TargetOffset 적용
	const FVector PivotWorld = WorldLocation + WorldRotation.RotateVector(TargetOffset);

	// 암이 뻗어 나갈 방향 (기본은 -X 축)
	FVector ArmDirection = WorldRotation.RotateVector(FVector(-1.0f, 0.0f, 0.0f)).GetSafeNormal();
	if (ArmDirection.IsZero())
	{
		ArmDirection = FVector(-1.0f, 0.0f, 0.0f);
	}

	const float DesiredLength = FMath::Max(0.0f, TargetArmLength);
	float ResolvedLength = DesiredLength;

	// 정밀 레이캐스트로 충돌 검사
	if (DesiredLength > KINDA_SMALL_NUMBER)
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			World = GWorld;
		}

		if (World)
		{
			if (UWorldPartitionManager* Partition = World->GetPartitionManager())
			{
				if (FBVHierarchy* BVH = Partition->GetBVH())
				{
					BVH->FlushRebuild();

					FRay Ray;
					Ray.Origin = PivotWorld;
					Ray.Direction = ArmDirection;

					float BestT = DesiredLength;
					AActor* HitActor = nullptr;
					TArray<AActor*> ExcludeList = { GetOwner() };
					BVH->QueryRayClosestStrict(Ray, HitActor, BestT, ExcludeList);

					if (HitActor && HitActor != GetOwner() && BestT < ResolvedLength)
					{
						ResolvedLength = FMath::Max(0.0f, BestT - BackoffEpsilon);
					}
				}
			}
		}
	}

	const FVector RawSocketWorldLocation = PivotWorld + ArmDirection * ResolvedLength + WorldRotation.RotateVector(SocketOffset);

	FVector FinalSocketPos = RawSocketWorldLocation;

	if (bEnableLag && DeltaTime > 0.0f)
	{
		// 첫 프레임 초기화
		if (!bInitLag) {
			SmoothedSocketPosWS = RawSocketWorldLocation;
			bInitLag = true;
		}

		const bool bHit = (ResolvedLength + BackoffEpsilon) < DesiredLength; // 히트 중일 때는 랙 적용 안 함
		if (bHit)
		{
			SmoothedSocketPosWS = RawSocketWorldLocation;
		}
		else
		{
			// alpha: 1 - exp(-k*dt)
			const float alpha = FMath::Clamp(-std::expm1f(-LagSpeed * DeltaTime), 0.0f, 1.0f);
			SmoothedSocketPosWS = FMath::Lerp(SmoothedSocketPosWS, RawSocketWorldLocation, alpha);
			
		}

		if ((SmoothedSocketPosWS - RawSocketWorldLocation).SizeSquared() > MaxLagDistance * MaxLagDistance)
		{
			// 최대 거리 제한
			FVector Dir = (RawSocketWorldLocation - SmoothedSocketPosWS).GetSafeNormal();
			SmoothedSocketPosWS = RawSocketWorldLocation - Dir * MaxLagDistance;
		}

		FinalSocketPos = SmoothedSocketPosWS;
	}

	CachedSocketWorld = FTransform(FinalSocketPos, WorldRotation, WorldTransform.Scale3D);
	bSocketValid = true;
}

void USpringArmComponent::DuplicateSubObjects()
{
	Super_t::DuplicateSubObjects();

	// 캐시 무효화
	bSocketValid = false;
	CachedSocketWorld = FTransform();

	// 랙 상태 초기화 (부드러운 시작을 위해)
	bInitLag = false;
	SmoothedSocketPosWS = FVector();
}