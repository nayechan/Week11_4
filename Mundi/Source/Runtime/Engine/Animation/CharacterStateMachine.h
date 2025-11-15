#pragma once
#include "AnimationStateMachine.h"
#include "UCharacterStateMachine.generated.h"

UCLASS(DisplayName="캐릭터 스테이트 머신", Description="ProcessState 패턴을 사용하는 예제 StateMachine")
class UCharacterStateMachine : public UAnimStateMachine
{
public:
	GENERATED_REFLECTION_BODY()

	UCharacterStateMachine() = default;
	virtual ~UCharacterStateMachine() = default;

	UPROPERTY(LuaReadWrite)
	float Speed = 0.0f;

	UPROPERTY(LuaReadWrite)
	bool bIsInAir = false;

	UPROPERTY(LuaReadWrite)
	bool bIsCombatMode = false;

	virtual void ProcessState() override;
};
