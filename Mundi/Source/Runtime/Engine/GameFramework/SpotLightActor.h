#pragma once

#include "Actor.h"
#include "ASpotLightActor.generated.h"

class USpotLightComponent;

UCLASS(DisplayName="스포트 라이트", Description="원뿔 모양의 조명을 생성하는 액터입니다")
class ASpotLightActor : public AActor
{
public:

	GENERATED_REFLECTION_BODY()

	ASpotLightActor();
protected:
	~ASpotLightActor() override;

public:
	USpotLightComponent* GetLightComponent() const { return LightComponent; }

	void DuplicateSubObjects() override;

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
	USpotLightComponent* LightComponent;
};
