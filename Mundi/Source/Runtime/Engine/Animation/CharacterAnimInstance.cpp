#include "pch.h"
#include "CharacterAnimInstance.h"
#include "AnimSequence.h"
#include "AnimationTypes.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "GlobalConsole.h"

void UCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	// ========================================
	// 올바른 실행 순서:
	// 1. Super 호출 - 시간 업데이트
	// 2. 게임 로직 (Movement, StateMachine 조건 체크)
	// 3. StateMachine 업데이트 (Transition 처리)
	//
	// Notify는 UpdateAnimation()에서 자동으로 트리거됨!
	// ========================================

	// 1. 시간 업데이트 (부모 클래스)
	Super::NativeUpdateAnimation(DeltaSeconds);

	// 2. 게임 로직
	UpdateMovementVariables();
	// ⚠️ UpdateStateMachine() 호출 비활성화
	// 테스트 시나리오에서는 TestAnimNotifyActor::Tick()에서 명시적으로 전환하므로
	// Speed 기반 자동 전환은 충돌을 일으킵니다.
	// UpdateStateMachine();

	// 3. StateMachine 업데이트 (Transition만 처리)
	if (StateMachine)
	{
		StateMachine->Update(DeltaSeconds);
	}

	// Notify는 UpdateAnimation()에서 자동 트리거됨 (직접 호출 불필요!)
}

void UCharacterAnimInstance::UpdateMovementVariables()
{
	if (!OwnerComponent)
		return;

	// TODO: Actor에서 Velocity 가져오기
	Speed = 0.0f;

	bIsInAir = false;
}

void UCharacterAnimInstance::UpdateStateMachine()
{
	if (!StateMachine)
		return;

	FName CurrentState = StateMachine->GetCurrentState();

	if (Speed > 10.0f)
	{
		StateMachine->TransitionTo("Walk");
	}
	else
	{
		StateMachine->TransitionTo("Idle");
	}
}

void UCharacterAnimInstance::GetAnimationPose(FPoseContext& OutPose)
{
	if (StateMachine)
	{
		// Unreal 방식: DeltaTime만 전파, 각 노드가 자신의 시간 관리
		// StateMachine이 내부적으로 StateLocalTime을 사용하여:
		// 1. 포즈 추출
		// 2. Notify 수집 -> OutPose.AnimNotifies에 추가
		StateMachine->GetBlendedPose(OutPose);
	}
	else
	{
		OutPose.BoneTransforms.Empty();
	}
}

// ⭐ GetActiveAnimations() 제거
// Unreal 방식에서는 AnimInstance가 직접 Notify를 찾지 않음
// 대신 GetAnimationPose() 호출 시 StateMachine이 Notify를 수집하여
// OutPose.AnimNotifies에 추가함 (트리 누적 패턴)
