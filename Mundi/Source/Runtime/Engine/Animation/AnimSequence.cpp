#include "pch.h"
#include "AnimSequence.h"
#include "GlobalConsole.h"
#include "Source/Runtime/Core/Misc/VertexData.h" 

void UAnimSequence::GetAnimationPose(FPoseContext& OutPose, const FAnimExtractContext& Context)
{
	// 스켈레톤이 없으면 실패
	if (!Skeleton)
	{
		UE_LOG("UAnimSequence::GetAnimationPose - No skeleton assigned");
		return;
	}

	// 본 개수만큼 포즈 초기화
	const int32 NumBones = Skeleton->Bones.Num();
	OutPose.SetNumBones(NumBones);

	// 모든 본에 대해 애니메이션 트랙에서 현재 시간의 트랜스폼 추출
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		OutPose.BoneTransforms[BoneIndex] = GetBoneTransformAtTime(BoneIndex, Context.CurrentTime);
	}
}

FTransform UAnimSequence::GetBoneTransformAtTime(int32 BoneIndex, float Time) const
{
	// 스켈레톤 범위 체크
	if (!Skeleton || BoneIndex < 0 || BoneIndex >= Skeleton->Bones.Num())
	{
		return FTransform();
	}

	// BoneAnimationTracks 배열에서 BoneTreeIndex로 매칭되는 트랙 찾기
	const FBoneAnimationTrack* Track = nullptr;
	for (const FBoneAnimationTrack& T : BoneAnimationTracks)
	{
		if (T.BoneTreeIndex == BoneIndex)
		{
			Track = &T;
			break;
		}
	}

	// 애니메이션 트랙이 없으면 identity (T-Pose 유지)
	if (!Track || Track->InternalTrack.IsEmpty())
	{
		return FTransform();
	}

	const FRawAnimSequenceTrack& RawTrack = Track->InternalTrack;

	// 각 컴포넌트 보간
	FVector Position = InterpolatePosition(RawTrack.PosKeys, Time);
	FQuat Rotation = InterpolateRotation(RawTrack.RotKeys, Time);
	FVector Scale = InterpolateScale(RawTrack.ScaleKeys, Time);

	return FTransform(Position, Rotation, Scale);
}

FVector UAnimSequence::InterpolatePosition(const TArray<FVector>& Keys, float Time) const
{
	if (Keys.IsEmpty())
		return FVector(0, 0, 0);

	if (Keys.Num() == 1)
		return Keys[0]; // 상수 트랙

	// 프레임 인덱스 계산
	const float FrameTime = Time * FrameRate.AsDecimal();
	const int32 Frame0 = FMath::Clamp(static_cast<int32>(FrameTime), 0, Keys.Num() - 1);
	const int32 Frame1 = FMath::Clamp(Frame0 + 1, 0, Keys.Num() - 1);
	const float Alpha = FMath::Frac(FrameTime);

	// 선형 보간
	return FMath::Lerp(Keys[Frame0], Keys[Frame1], Alpha);
}

FQuat UAnimSequence::InterpolateRotation(const TArray<FQuat>& Keys, float Time) const
{
	if (Keys.IsEmpty())
		return FQuat();

	if (Keys.Num() == 1)
		return Keys[0]; // 상수 트랙

	// 프레임 인덱스 계산
	const float FrameTime = Time * FrameRate.AsDecimal();
	const int32 Frame0 = FMath::Clamp(static_cast<int32>(FrameTime), 0, Keys.Num() - 1);
	const int32 Frame1 = FMath::Clamp(Frame0 + 1, 0, Keys.Num() - 1);
	const float Alpha = FMath::Frac(FrameTime);

	// Spherical Linear Interpolation (Slerp)
	return FQuat::Slerp(Keys[Frame0], Keys[Frame1], Alpha);
}

FVector UAnimSequence::InterpolateScale(const TArray<FVector>& Keys, float Time) const
{
	if (Keys.IsEmpty())
		return FVector(1, 1, 1);

	if (Keys.Num() == 1)
		return Keys[0]; // 상수 트랙

	// 프레임 인덱스 계산
	const float FrameTime = Time * FrameRate.AsDecimal();
	const int32 Frame0 = FMath::Clamp(static_cast<int32>(FrameTime), 0, Keys.Num() - 1);
	const int32 Frame1 = FMath::Clamp(Frame0 + 1, 0, Keys.Num() - 1);
	const float Alpha = FMath::Frac(FrameTime);

	// 선형 보간
	return FMath::Lerp(Keys[Frame0], Keys[Frame1], Alpha);
}

void UAnimSequence::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	// TODO: BoneAnimationTracks 직렬화
	// TODO: FrameRate, NumberOfFrames, NumberOfKeys 직렬화
}
