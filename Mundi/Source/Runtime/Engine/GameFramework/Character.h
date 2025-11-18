#pragma once
#include "Pawn.h"
#include "ACharacter.generated.h"


UCLASS(DisplayName = "ACharacter", Description = "Character Actor")
class ACharacter : public APawn
{

	GENERATED_REFLECTION_BODY();
public:
	ACharacter() = default;

};