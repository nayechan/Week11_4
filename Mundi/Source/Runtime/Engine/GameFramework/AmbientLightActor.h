#pragma once

#include "Actor.h"
#include "AAmbientLightActor.generated.h"

class UAmbientLightComponent;

UCLASS(DisplayName="앰비언트 라이트", Description="전역 조명을 생성하는 액터입니다")
class AAmbientLightActor : public AActor
{
public:
	GENERATED_REFLECTION_BODY()

	AAmbientLightActor();
protected:
	~AAmbientLightActor() override;

public:
	UAmbientLightComponent* GetLightComponent() const { return LightComponent; }

	void DuplicateSubObjects() override;

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

protected:
	UAmbientLightComponent* LightComponent;
};
