#pragma once
#include "AnimGraphInstance.h"
#include "AnimNode_BlendSpace1D.h"
#include "UCharacterAnimInstance.generated.h"

class ACharacter;
UCLASS(DisplayName="캐릭터 애니메이션 인스턴스", Description="State Machine을 사용하는 예제 AnimInstance")
class UCharacterAnimInstance : public UAnimGraphInstance
{
public:
	GENERATED_REFLECTION_BODY()

	UCharacterAnimInstance() = default;
	virtual ~UCharacterAnimInstance() = default;

	// 애니메이션 시퀀스들
	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Animations", Tooltip="Idle 상태 애니메이션")
	class UAnimSequence* IdleAnimation = nullptr;

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Animations", Tooltip="Walk 상태 애니메이션")
	class UAnimSequence* WalkAnimation = nullptr;

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Animations", Tooltip="Run 상태 애니메이션")
	class UAnimSequence* RunAnimation = nullptr;

	// BlendSpace1D Node (Locomotion)
	UPROPERTY()
	UAnimNode_BlendSpace1D* LocomotionBlendSpace = nullptr;

	// 게임플레이 변수들
	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Movement")
	float Speed = 0.0f;

	// 부드러운 속도 전환을 위한 보간된 속도
	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Movement")
	float SmoothedSpeed = 0.0f;

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Movement", Tooltip="속도 보간 비율 (5.0~10.0 권장)")
	float SmoothRate = 5.0f;

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Movement")
	bool bIsInAir = false;

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Combat")
	bool bIsCombatMode = false;

	virtual void NativeInitializeAnimation() override;

	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected:
	virtual void UpdateMovementVariables();
	virtual void UpdateSmoothedSpeed(float DeltaSeconds);

	// Character 상태 얻어와서 애니메이션 인풋으로 사용
	ACharacter* Character = nullptr;
};
