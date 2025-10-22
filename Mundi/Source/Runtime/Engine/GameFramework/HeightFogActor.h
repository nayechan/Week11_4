#pragma once
#include "Info.h"


class AHeightFogActor : public AInfo
{
public:
	DECLARE_CLASS(AHeightFogActor, AInfo)
	GENERATED_REFLECTION_BODY()

	AHeightFogActor();

	DECLARE_DUPLICATE(AHeightFogActor)

	void DuplicateSubObjects() override;
};