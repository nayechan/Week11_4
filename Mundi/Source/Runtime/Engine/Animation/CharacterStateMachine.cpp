#include "pch.h"
#include "CharacterStateMachine.h"

void UCharacterStateMachine::ProcessState()
{
	FName Current = GetCurrentState();

	if (bIsInAir)
	{
		TransitionTo("Jump");
		return;
	}

	if (bIsCombatMode)
	{
		if (Speed > 10.0f)
		{
			TransitionTo("CombatWalk");
		}
		else
		{
			TransitionTo("CombatIdle");
		}
	}
	else
	{
		if (Speed > 300.0f)
		{
			TransitionTo("Run");
		}
		else if (Speed > 10.0f)
		{
			TransitionTo("Walk");
		}
		else
		{
			TransitionTo("Idle");
		}
	}
}
