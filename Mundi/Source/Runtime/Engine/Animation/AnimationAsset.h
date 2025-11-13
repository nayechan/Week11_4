#pragma once
#include "ResourceBase.h"
#include "AnimationTypes.h"
#include "UAnimationAsset.generated.h"

UCLASS(DisplayName="애니메이션 에셋", Description="애니메이션 에셋 베이스 클래스")
class UAnimationAsset : public UResourceBase
{
public:
	GENERATED_REFLECTION_BODY()

	UAnimationAsset() = default;
	virtual ~UAnimationAsset() = default;

	// 애니메이션 길이 반환 (순수 가상)
	virtual float GetPlayLength() const { return 0.0f; }

	// 스켈레톤 참조
	UPROPERTY(EditAnywhere, Category="[애니메이션]", Tooltip="대상 스켈레톤")
	class FSkeleton* Skeleton = nullptr;

	// 메타데이터
	UPROPERTY(EditAnywhere, Category="[애니메이션]")
	TArray<class UAnimMetaData*> MetaData;

	// 직렬화
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
