#include "pch.h"
#include "AnimationRuntime.h"
#include "Source/Runtime/Core/Math/Vector.h"

void FAnimationRuntime::BlendTwoPosesTogether(
	const FPoseContext& PoseA,
	const FPoseContext& PoseB,
	float BlendAlpha,
	FPoseContext& OutPose)
{
	// 본 개수 체크
	const int32 NumBones = FMath::Min(PoseA.GetNumBones(), PoseB.GetNumBones());
	OutPose.SetNumBones(NumBones);

	// 각 본의 트랜스폼 블렌딩
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		OutPose.BoneTransforms[BoneIndex] = BlendTransforms(
			PoseA.BoneTransforms[BoneIndex],
			PoseB.BoneTransforms[BoneIndex],
			BlendAlpha
		);
	}
}

FTransform FAnimationRuntime::BlendTransforms(
	const FTransform& A,
	const FTransform& B,
	float Alpha)
{
	// Alpha 클램핑
	Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

	// Position: 선형 보간
	FVector BlendedPosition = FMath::Lerp(A.Translation, B.Translation, Alpha);

	// Rotation: Spherical Linear Interpolation (Slerp)
	FQuat BlendedRotation = FQuat::Slerp(A.Rotation, B.Rotation, Alpha);

	// Scale: 선형 보간
	FVector BlendedScale = FMath::Lerp(A.Scale3D, B.Scale3D, Alpha);

	return FTransform(BlendedPosition, BlendedRotation, BlendedScale);
}
