#pragma once
#include "AnimationAsset.h"
#include "UAnimSequenceBase.generated.h"

UCLASS(Abstract)
class UAnimSequenceBase : public UAnimationAsset
{
public:
	GENERATED_REFLECTION_BODY()

	UAnimSequenceBase() = default;
	~UAnimSequenceBase() = default;

	// Notify 이벤트 배열 (발제 문서 요구사항)
	UPROPERTY(LuaReadWrite, EditAnywhere, Category="[애니메이션|Notify]", Tooltip="애니메이션 알림 이벤트")
	TArray<FAnimNotifyEvent> Notifies;

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="[애니메이션]", Tooltip="애니메이션 길이 (초)")
	float SequenceLength = 0.0f;

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="[애니메이션]", Tooltip="재생 속도 배율", Range="0.1, 10.0")
	float RateScale = 1.0f;

	// 포즈 추출
	virtual void GetAnimationPose(FPoseContext& OutPose, const FAnimExtractContext& Context) = 0;

	// 시간 범위 내의 Notify 가져오기
	void GetAnimNotifiesInRange(float StartTime, float EndTime, TArray<FAnimNotifyEvent>& OutNotifies) const;

	// 애니메이션 길이 반환
	virtual float GetPlayLength() const override { return SequenceLength; }

	// 직렬화
	virtual void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};
