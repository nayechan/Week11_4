#pragma once
#include "AnimInstance.h"
#include "AnimationStateMachine.h"
#include "UCharacterAnimInstance.generated.h"

UCLASS(DisplayName="캐릭터 애니메이션 인스턴스", Description="State Machine을 사용하는 예제 AnimInstance")
class UCharacterAnimInstance : public UAnimInstance
{
public:
	GENERATED_REFLECTION_BODY()

	UCharacterAnimInstance() = default;
	virtual ~UCharacterAnimInstance() = default;

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="State Machine")
	UAnimStateMachine* StateMachine = nullptr;

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Movement")
	float Speed = 0.0f;

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Movement")
	bool bIsInAir = false;

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Combat")
	bool bIsCombatMode = false;

	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	virtual void GetAnimationPose(struct FPoseContext& OutPose) override;

	virtual void GetActiveAnimations(TArray<class UAnimSequence*>& OutAnimations) const override;

protected:
	virtual void UpdateMovementVariables();
	virtual void UpdateStateMachine();
};
