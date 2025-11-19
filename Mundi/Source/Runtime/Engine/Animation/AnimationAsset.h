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

	// 애니메이션 길이 반환
	virtual float GetPlayLength() const { return 0.0f; }

	// 스켈레톤 고유 ID (호환성 판별용)
	UPROPERTY(EditAnywhere, Category="[애니메이션]", Tooltip="스켈레톤 고유 ID - 같은 ID를 가진 스켈레탈 메시와 호환됩니다")
	FString SkeletonID;

	// 스켈레톤 참조
	UPROPERTY(LuaReadWrite, EditAnywhere, Category="[애니메이션]", Tooltip="대상 스켈레톤")
	struct FSkeleton* Skeleton = nullptr;

	// 메타데이터
	UPROPERTY(LuaReadWrite, EditAnywhere, Category="[애니메이션]")
	TArray<class UAnimMetaData*> MetaData;

	// 직렬화
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;

	// 오버라이드 데이터 저장/로드 (skeleton + 기타 메타데이터)
	bool SaveOverrideData(const FString& FilePath);
	bool LoadOverrideData(const FString& FilePath);
};
