#pragma once
#include "StaticMeshActor.h"

class UPointLightComponent; 
class URotatingMovementComponent;

class AFireBallActor : public AStaticMeshActor
{
public:
	DECLARE_CLASS(AFireBallActor, AActor)
	GENERATED_REFLECTION_BODY()

	AFireBallActor();

protected:
	~AFireBallActor();

protected:
	UPointLightComponent* PointLightComponent;  
	URotatingMovementComponent* RotatingComponent;

};