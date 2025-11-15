#include "pch.h"
#include "AnimInstance.h"
#include "AnimSequence.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "GlobalConsole.h"

// ========================================
// 메인 업데이트 파이프라인
// ========================================

void UAnimInstance::UpdateAnimation(float DeltaSeconds)
{
	// 1. C++ 네이티브 업데이트
	NativeUpdateAnimation(DeltaSeconds);

	// 2. TODO: Lua 스크립트 업데이트 (향후 구현)
	// LuaUpdateAnimation(DeltaSeconds);
}

// ========================================
// C++ 네이티브 업데이트 (오버라이드 가능)
// ========================================

void UAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	// 1. 시간 업데이트
	PreviousTime = CurrentTime;
	CurrentTime += DeltaSeconds;

	// 2. Notify 트리거
	TriggerAnimNotifies(DeltaSeconds);

	// 3. TODO: State Machine 업데이트
	// if (StateMachine)
	// {
	//     StateMachine->Update(DeltaSeconds);
	// }

	// 하위 클래스에서 Super::NativeUpdateAnimation() 호출 후 커스텀 로직 추가
}

void UAnimInstance::GetAnimationPose(FPoseContext& OutPose)
{
	OutPose.BoneTransforms.Empty();
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
