#include "pch.h"
#include "AnimInstance.h"
#include "AnimSequence.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "GlobalConsole.h"

// ========================================
// 초기화 파이프라인
// ========================================

void UAnimInstance::InitializeAnimation()
{
	// ========================================
	// 애니메이션 초기화 파이프라인 (Unreal 방식)
	// ========================================

	// 1. C++ 네이티브 초기화 (하위 클래스에서 오버라이드)
	//    StateMachine 생성, State/Transition 추가 등
	NativeInitializeAnimation();

	// 2. Lua 스크립트 초기화 (ULuaAnimInstance에서 구현)
	LuaInitializeAnimation();
}

// ========================================
// C++ 네이티브 초기화 (오버라이드 가능)
// ========================================

void UAnimInstance::NativeInitializeAnimation()
{
	// Unreal 방식: 기본 구현은 비어있음
	// 하위 클래스(예: CharacterAnimInstance)에서:
	// 1. Super::NativeInitializeAnimation() 호출 (선택)
	// 2. StateMachine 생성 및 초기화
	// 3. States 추가 (각 State에 AnimSequence 연결)
	// 4. Transitions 추가 (조건 및 블렌드 시간 설정)
	// 5. 초기 상태 설정
}

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

	// 2. Lua 스크립트 업데이트 (ULuaAnimInstance에서 구현)
	LuaUpdateAnimation(DeltaSeconds);

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

void UAnimInstance::LuaInitializeAnimation()
{
	// 기본 구현 비어있음 (ULuaAnimInstance에서 override)
}

void UAnimInstance::LuaUpdateAnimation(float DeltaSeconds)
{
	// 기본 구현 비어있음 (ULuaAnimInstance에서 override)
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

UWorld* UAnimInstance::GetWorld() const
{
	// Outer 체인 대신 OwnerComponent를 통해 World 접근
	// AnimInstance는 Component가 아니므로 Owner 대신 OwnerComponent 사용
	return OwnerComponent ? OwnerComponent->GetWorld() : nullptr;
}
