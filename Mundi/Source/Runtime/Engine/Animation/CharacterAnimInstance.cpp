#include "pch.h"
#include "CharacterAnimInstance.h"
#include "AnimSequence.h"
#include "AnimationTypes.h"
#include "Source/Runtime/Engine/Components/SkeletalMeshComponent.h"
#include "GlobalConsole.h"

void UCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	UpdateMovementVariables();

	UpdateStateMachine();

	if (StateMachine)
	{
		StateMachine->Update(DeltaSeconds);
	}
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
		StateMachine->GetBlendedPose(GetCurrentTime(), OutPose);
	}
	else
	{
		OutPose.BoneTransforms.Empty();
	}
}
