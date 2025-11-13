#pragma once

#include "StaticMeshActor.h"
#include "AFireBallActor.generated.h"

class UPointLightComponent; 
class URotatingMovementComponent;

UCLASS(DisplayName="파이어볼", Description="파이어볼 이펙트 액터입니다")
class AFireBallActor : public AStaticMeshActor
{
	//=
public:

	GENERATED_REFLECTION_BODY()

	AFireBallActor();

protected:
	~AFireBallActor();

protected:
	UPointLightComponent* PointLightComponent;  
	URotatingMovementComponent* RotatingComponent;
};