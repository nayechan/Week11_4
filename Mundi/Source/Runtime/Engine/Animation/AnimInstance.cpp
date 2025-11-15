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
	// ========================================
	// 애니메이션 업데이트 파이프라인 (Unreal 방식)
	// ========================================

	// 1. C++ 네이티브 업데이트 (하위 클래스에서 오버라이드)
	//    각 AnimNode가 내부 시간(InternalTime) 업데이트
	NativeUpdateAnimation(DeltaSeconds);

	// 2. TODO: Lua 스크립트 업데이트 (향후 구현)
	// LuaUpdateAnimation(DeltaSeconds);

	// 3. 포즈 추출 + Notify 수집 (트리 누적 패턴)
	//    GetAnimationPose()가 트리를 순회하며:
	//    - 각 노드가 포즈 계산
	//    - 각 노드가 Notify 수집하여 FPoseContext.AnimNotifies에 추가
	FPoseContext FinalPose;
	GetAnimationPose(FinalPose);

	// 4. 수집된 Notify 트리거 (프레임워크가 자동 처리)
	TriggerAnimNotifies(FinalPose);
}

// ========================================
// C++ 네이티브 업데이트 (오버라이드 가능)
// ========================================

void UAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	// Unreal 방식: 기본 구현은 비어있음
	// 하위 클래스(예: CharacterAnimInstance)에서:
	// 1. Super::NativeUpdateAnimation(DeltaSeconds) 호출 (선택)
	// 2. StateMachine->Update(DeltaSeconds) 호출 (각 노드가 InternalTime 업데이트)
	//
	// AnimInstance는 전역 시간을 관리하지 않음!
}

void UAnimInstance::GetAnimationPose(FPoseContext& OutPose)
{
	OutPose.BoneTransforms.Empty();
}

void UAnimInstance::TriggerAnimNotifies(const FPoseContext& Pose)
{
	if (!OwnerComponent)
		return;

	// Unreal 방식: FPoseContext에 이미 수집된 Notify들을 일괄 트리거
	// AnimNode들이 트리 순회 중 Pose.AnimNotifies에 추가한 것들
	for (const FAnimNotifyEvent& Notify : Pose.AnimNotifies)
	{
		OwnerComponent->HandleAnimNotify(Notify);
	}
}
