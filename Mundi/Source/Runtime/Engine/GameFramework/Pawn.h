#pragma once

#include "Actor.h"
#include "APawn.generated.h"

class APlayerController;

UCLASS(Abstract, DisplayName = "APawn", Description = "APawn Actor")
class APawn : public AActor
{

	GENERATED_REFLECTION_BODY()

public:

	APawn() = default;

	//virtual void AddInputVector(FVector WorldVector, bool bForce = false) {};
	// 앞뒤 인풋 처리
	virtual void HandleThrustInput(float InScalar) {};
	// 좌우 인풋 처리
	virtual void HandleSteerInput(float InScalar) {};
	// 부스터 인풋 처리
	virtual void HandleBoosterInput() {};
	
	bool IsMoveInputIgnored() const;


	TWeakPtr<APlayerController> Controller;
};