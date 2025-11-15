#pragma once
#include "AnimInstance.h"
#include "UAnimSingleNodeInstance.generated.h"

UCLASS(DisplayName="단일 애니메이션 인스턴스", Description="하나의 애니메이션만 재생")
class UAnimSingleNodeInstance : public UAnimInstance
{
public:
	GENERATED_REFLECTION_BODY()

	UAnimSingleNodeInstance() = default;
	virtual ~UAnimSingleNodeInstance() = default;

	// 애니메이션 설정
	void SetAnimationAsset(class UAnimSequence* NewAsset);

	// 재생 제어
	void Play(bool bInLooping = false);
	void Stop();
	void Pause();
	void SetPlayRate(float InPlayRate);

	// 재생 상태 확인
	bool IsPlaying() const { return bIsPlaying; }
	bool IsLooping() const { return bLooping; }
	float GetPlayRate() const { return PlayRate; }

	// 업데이트 구현
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// 포즈 추출
	virtual void GetAnimationPose(struct FPoseContext& OutPose) override;

private:
	class UAnimSequence* CurrentSequence = nullptr;
	bool bIsPlaying = false;
	bool bLooping = false;
	float PlayRate = 1.0f;
};
