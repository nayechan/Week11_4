#pragma once
#include "AnimInstance.h"
#include "AnimationStateMachine.h"
#include "UCharacterAnimInstance.generated.h"

class ACharacter;
UCLASS(DisplayName="캐릭터 애니메이션 인스턴스", Description="State Machine을 사용하는 예제 AnimInstance")
class UCharacterAnimInstance : public UAnimInstance
{
public:
	GENERATED_REFLECTION_BODY()

	UCharacterAnimInstance() = default;
	virtual ~UCharacterAnimInstance();

	// 애니메이션 시퀀스들
	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Animations", Tooltip="Idle 상태 애니메이션")
	class UAnimSequence* IdleAnimation = nullptr;

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Animations", Tooltip="Walk 상태 애니메이션")
	class UAnimSequence* WalkAnimation = nullptr;

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Animations", Tooltip="Run 상태 애니메이션")
	class UAnimSequence* RunAnimation = nullptr;

	// State Machine
	UPROPERTY(LuaReadWrite, EditAnywhere, Category="State Machine")
	UAnimStateMachine* StateMachine = nullptr;

	// 게임플레이 변수들
	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Movement")
	float Speed = 0.0f;

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Movement")
	bool bIsInAir = false;

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Combat")
	bool bIsCombatMode = false;

	virtual void NativeInitializeAnimation() override;

	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	virtual void GetAnimationPose(struct FPoseContext& OutPose) override;

protected:
	virtual void UpdateMovementVariables();
	virtual void UpdateStateMachine();

	// Character 상태 얻어와서 스테이트 머신 인풋으로 사용, 시퀀스 재생
	ACharacter* Character = nullptr;
};
