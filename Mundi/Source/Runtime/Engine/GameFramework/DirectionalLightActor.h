#pragma once

#include "Actor.h"
#include "ADirectionalLightActor.generated.h"

class UDirectionalLightComponent;

UCLASS(DisplayName="방향성 라이트", Description="태양광과 같은 평행광을 생성하는 액터입니다")
class ADirectionalLightActor : public AActor
{
	GENERATED_REFLECTION_BODY()

	ADirectionalLightActor();
protected:
	~ADirectionalLightActor() override;

public:
	UDirectionalLightComponent* GetLightComponent() const { return LightComponent; }

	void DuplicateSubObjects() override;

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
	UDirectionalLightComponent* LightComponent;
};
