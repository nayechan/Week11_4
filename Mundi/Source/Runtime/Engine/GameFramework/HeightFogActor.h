#pragma once

#include "Info.h"
#include "AHeightFogActor.generated.h"

UCLASS(DisplayName="높이 안개", Description="높이 기반 안개 효과를 생성하는 액터입니다")
class AHeightFogActor : public AInfo
{
public:

	GENERATED_REFLECTION_BODY()

	AHeightFogActor();

	void DuplicateSubObjects() override;
};