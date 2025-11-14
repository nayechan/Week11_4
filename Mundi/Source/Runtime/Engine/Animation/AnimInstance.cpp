#include "pch.h"
#include "AnimInstance.h"
#include "AnimSequence.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "GlobalConsole.h"

void UAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	// 기본 구현: 비어있음 (파생 클래스에서 오버라이드하여 구현)
	// 시간 업데이트는 파생 클래스(UAnimSingleNodeInstance 등)에서 처리
}

void UAnimInstance::TriggerAnimNotifies(float DeltaSeconds)
{
	if (!OwnerComponent)
	{
		return;
	}

	// TODO: 현재 재생 중인 애니메이션의 Notify 체크
	// TODO: PreviousTime ~ CurrentTime 범위의 Notify 트리거
	// TODO: OwnerComponent->HandleAnimNotify() 호출
}
