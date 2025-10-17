#pragma once
#include "Actor.h"

class UPointLightComponent;

class APointLightActor : public AActor
{
public:
	DECLARE_CLASS(APointLightActor, AActor)

	APointLightActor();
protected:
	~APointLightActor() override;

public:
	UPointLightComponent* GetLightComponent() const { return LightComponent; }

	void DuplicateSubObjects() override;
	DECLARE_DUPLICATE(APointLightActor)

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
	UPointLightComponent* LightComponent;
};
