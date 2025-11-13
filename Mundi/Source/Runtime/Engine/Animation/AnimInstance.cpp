#include "pch.h"
#include "AnimInstance.h"
#include "AnimSequence.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "GlobalConsole.h"

void UAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	// 기본 구현: 시간만 업데이트
	PreviousTime = CurrentTime;
	CurrentTime += DeltaSeconds;

	// 팀원2가 오버라이드하여 커스텀 로직 구현
}

void UAnimInstance::TriggerAnimNotifies(float DeltaSeconds)
{
	if (!OwnerComponent)
		return;

	// TODO: 현재 재생 중인 애니메이션의 Notify 체크
	// TODO: PreviousTime ~ CurrentTime 범위의 Notify 트리거
	// TODO: OwnerComponent->HandleAnimNotify() 호출

	// 팀원4가 상세 구현할 예정
}
