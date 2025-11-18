#pragma once

#include "Actor.h"
#include "APawn.generated.h"

UCLASS(Abstract, DisplayName = "APawn", Description = "APawn Actor")
class APawn : public AActor
{

	GENERATED_REFLECTION_BODY()

public:

	APawn() = default;

	// 앞뒤 인풋 처리
	virtual void HandleThrustInput(float InValue) {};
	// 좌우 인풋 처리
	virtual void HandleSteerInput(float InValue) {};
	// 부스터 인풋 처리
	virtual void HandleBoosterInput() {};
};