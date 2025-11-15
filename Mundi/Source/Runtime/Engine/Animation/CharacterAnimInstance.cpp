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

void UCharacterAnimInstance::GetActiveAnimations(TArray<UAnimSequence*>& OutAnimations) const
{
	OutAnimations.Empty();

	if (!StateMachine)
		return;

	if (StateMachine->IsTransitioning())
	{
		// Transition 중: From과 To 애니메이션 둘 다 추가
		UAnimSequence* FromAnim = StateMachine->GetFromAnimation();
		UAnimSequence* ToAnim = StateMachine->GetToAnimation();

		if (FromAnim)
			OutAnimations.Add(FromAnim);
		if (ToAnim)
			OutAnimations.Add(ToAnim);
	}
	else
	{
		// 일반 재생: 현재 애니메이션만
		UAnimSequence* CurrentAnim = StateMachine->GetCurrentAnimation();
		if (CurrentAnim)
			OutAnimations.Add(CurrentAnim);
	}
}
