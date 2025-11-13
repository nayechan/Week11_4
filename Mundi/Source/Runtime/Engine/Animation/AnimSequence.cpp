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

	// 현재는 빈 구현: 모든 본을 identity transform으로 설정
	// TODO: Context.CurrentTime에 맞춰 실제 애니메이션 트랙에서 보간

	// 각 본에 대해 애니메이션 적용
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		if (BoneIndex < BoneAnimationTracks.Num())
		{
			OutPose.BoneTransforms[BoneIndex] = GetBoneTransformAtTime(BoneIndex, Context.CurrentTime);
		}
		else
		{
			OutPose.BoneTransforms[BoneIndex] = FTransform();
		}
	}
}

FTransform UAnimSequence::GetBoneTransformAtTime(int32 BoneIndex, float Time) const
{
	// 인덱스 범위 체크
	if (BoneIndex < 0 || BoneIndex >= BoneAnimationTracks.Num())
	{
		return FTransform();
	}

	const FBoneAnimationTrack& Track = BoneAnimationTracks[BoneIndex];
	const FRawAnimSequenceTrack& RawTrack = Track.InternalTrack;

	// 빈 트랙이면 identity
	if (RawTrack.IsEmpty())
	{
		return FTransform();
	}

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
	const float Alpha = 1.0f / FrameTime;

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
	const float Alpha = 1.0f / FrameTime;

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
	const float Alpha = 1.0f / FrameTime;

	// 선형 보간
	return FMath::Lerp(Keys[Frame0], Keys[Frame1], Alpha);
}

void UAnimSequence::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	// TODO: BoneAnimationTracks 직렬화
	// TODO: FrameRate, NumberOfFrames, NumberOfKeys 직렬화
}
