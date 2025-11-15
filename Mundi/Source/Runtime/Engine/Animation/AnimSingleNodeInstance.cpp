#include "pch.h"
#include "AnimSingleNodeInstance.h"
#include "AnimSequence.h"
#include "GlobalConsole.h"

void UAnimSingleNodeInstance::SetAnimationAsset(UAnimSequence* NewAsset)
{
	CurrentSequence = NewAsset;
	InternalTime = 0.0f;
	PreviousInternalTime = 0.0f;
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
	InternalTime = 0.0f;
	PreviousInternalTime = 0.0f;

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
	// Unreal 방식: DeltaTime만 받아서 InternalTime 업데이트
	if (!bIsPlaying || !CurrentSequence)
		return;

	// Notify 범위 체크용
	PreviousInternalTime = InternalTime;
	InternalTime += DeltaSeconds * PlayRate;

	// 애니메이션 길이 체크
	const float AnimLength = CurrentSequence->GetPlayLength();

	if (InternalTime >= AnimLength)
	{
		if (bLooping)
		{
			// 루핑: 시작으로 돌아가기
			InternalTime = std::fmod(InternalTime, AnimLength);
		}
		else
		{
			// 비루핑: 정지
			InternalTime = AnimLength;
			bIsPlaying = false;

			UE_LOG("UAnimSingleNodeInstance - Animation finished");
		}
	}
}

void UAnimSingleNodeInstance::GetAnimationPose(FPoseContext& OutPose)
{
	if (!CurrentSequence)
	{
		OutPose.BoneTransforms.Empty();
		return;
	}

	// Unreal 방식:
	// 1. InternalTime 기준으로 포즈 추출
	// 2. Notify 수집하여 OutPose.AnimNotifies에 추가
	FAnimExtractContext Context(InternalTime, bLooping);
	CurrentSequence->GetAnimationPose(OutPose, Context);

	// Notify 수집 (트리 누적 패턴)
	TArray<FAnimNotifyEvent> Notifies;
	CurrentSequence->GetAnimNotifiesInRange(PreviousInternalTime, InternalTime, Notifies);
	OutPose.AnimNotifies.Append(Notifies);
}
