#include "pch.h"
#include "AnimNode_StateMachine.h"
#include "AnimationTypes.h"
#include "GlobalConsole.h"

// ========================================
// UAnimNode_StateMachine 구현
// ========================================

void UAnimNode_StateMachine::Initialize(const FSkeleton* InSkeleton, UAnimInstance* InOwner)
{
	// 부모 초기화 (Skeleton, Owner 저장)
	Super::Initialize(InSkeleton, InOwner);

	if (!StateMachine)
	{
		UE_LOG("AnimNode_StateMachine::Initialize - StateMachine is nullptr! Set StateMachine before initialization.");
		return;
	}

	// StateMachine 초기화
	// NOTE: UAnimStateMachine::Initialize()는 인자가 없음
	// 이미 외부에서 AddState, AddTransition, SetInitialState 완료 가정
	StateMachine->Initialize();

	UE_LOG("AnimNode_StateMachine::Initialize - StateMachine initialized with initial state: %s",
		StateMachine->GetCurrentState().ToString().c_str());
}

void UAnimNode_StateMachine::Update(float DeltaTime)
{
	if (!StateMachine)
	{
		return;
	}

	// StateMachine에 시간 업데이트 위임
	// Transition 처리, State InternalTime 업데이트
	StateMachine->Update(DeltaTime);
}

void UAnimNode_StateMachine::Evaluate(FPoseContext& OutPose)
{
	if (StateMachine)
	{
		// StateMachine에 포즈 추출 위임
		// GetBlendedPose는:
		// 1. 현재 상태 애니메이션 추출
		// 2. Transition 중이면 FromState + ToState 블렌딩
		// 3. OutPose.AnimNotifies에 Notify 수집
		StateMachine->GetBlendedPose(OutPose);
	}
	else
	{
		// Fallback: RefPose 반환
		if (Skeleton)
		{
			OutPose.SetNumBones(Skeleton->Bones.Num());
			// Identity transform으로 초기화됨 (FPoseContext::SetNumBones 내부)
		}
		else
		{
			// Skeleton도 없으면 빈 포즈
			OutPose.BoneTransforms.Empty();
		}

		UE_LOG("AnimNode_StateMachine::Evaluate - StateMachine is nullptr, returning RefPose");
	}
}

FString UAnimNode_StateMachine::GetNodeName() const
{
	if (StateMachine)
	{
		FName CurrentState = StateMachine->GetCurrentState();
		return "StateMachine(" + CurrentState.ToString() + ")";
	}

	return "StateMachine(None)";
}
