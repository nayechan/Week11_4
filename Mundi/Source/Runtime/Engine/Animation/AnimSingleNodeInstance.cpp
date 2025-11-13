#include "pch.h"
#include "AnimSingleNodeInstance.h"
#include "AnimSequence.h"
#include "GlobalConsole.h"

void UAnimSingleNodeInstance::SetAnimationAsset(UAnimSequence* NewAsset)
{
	CurrentSequence = NewAsset;
	CurrentTime = 0.0f;
	PreviousTime = 0.0f;
}

void UAnimSingleNodeInstance::Play(bool bInLooping)
{
	bIsPlaying = true;
	bLooping = bInLooping;

	UE_LOG("UAnimSingleNodeInstance::Play - Looping: %d", bLooping ? 1 : 0);
}

void UAnimSingleNodeInstance::Stop()
{
	bIsPlaying = false;
	CurrentTime = 0.0f;
	PreviousTime = 0.0f;

	UE_LOG("UAnimSingleNodeInstance::Stop");
}

void UAnimSingleNodeInstance::Pause()
{
	bIsPlaying = false;

	UE_LOG("UAnimSingleNodeInstance::Pause");
}

void UAnimSingleNodeInstance::SetPlayRate(float InPlayRate)
{
	PlayRate = FMath::Max(0.1f, InPlayRate); // 최소 0.1x
}

void UAnimSingleNodeInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!bIsPlaying || !CurrentSequence)
		return;

	// 시간 업데이트
	PreviousTime = CurrentTime;
	CurrentTime += DeltaSeconds * PlayRate;

	// 애니메이션 길이 체크
	const float AnimLength = CurrentSequence->GetPlayLength();

	if (CurrentTime >= AnimLength)
	{
		if (bLooping)
		{
			// 루핑: 시작으로 돌아가기
			CurrentTime = std::fmod(CurrentTime, AnimLength);
		}
		else
		{
			// 비루핑: 정지
			CurrentTime = AnimLength;
			bIsPlaying = false;

			UE_LOG("UAnimSingleNodeInstance - Animation finished");
		}
	}

	// Notify 트리거
	TriggerAnimNotifies(DeltaSeconds);
}
