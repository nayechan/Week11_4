#pragma once

#include "Actor.h"
#include "AEmptyActor.generated.h"

UCLASS(DisplayName="빈 액터", Description="기본 빈 액터입니다")
class AEmptyActor : public AActor
{
public:

	GENERATED_REFLECTION_BODY()

	AEmptyActor();
	~AEmptyActor() override = default;

	void DuplicateSubObjects() override;

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
