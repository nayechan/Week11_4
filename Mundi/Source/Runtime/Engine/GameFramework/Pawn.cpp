#include "pch.h"
#include "Pawn.h"
#include "PlayerController.h"

bool APawn::IsMoveInputIgnored() const
{
	if (Controller.IsValid())
	{
		return Controller.Get()->IsMoveInputIgnored();
	}
}