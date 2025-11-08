#pragma once

#include "Actor.h"
#include "AFakeSpotLightActor.generated.h"

class UBillboardComponent;
class UPerspectiveDecalComponent;

UCLASS(DisplayName="가짜 스포트 라이트", Description="테스트용 스포트 라이트 액터입니다")
class AFakeSpotLightActor : public AActor
{
public:

	GENERATED_REFLECTION_BODY()

	AFakeSpotLightActor();
protected:
	~AFakeSpotLightActor() override;

public:
	UBillboardComponent* GetBillboardComponent() const { return BillboardComponent; }
	UPerspectiveDecalComponent* GetDecalComponent() const { return DecalComponent; }

	void DuplicateSubObjects() override;

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
protected:
	UBillboardComponent* BillboardComponent{};
	UPerspectiveDecalComponent* DecalComponent{};
};
