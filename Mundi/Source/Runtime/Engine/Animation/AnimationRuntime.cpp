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

float FAnimationRuntime::EvaluateBezierCurve(const float Curve[4], float T)
{
	// T 클램핑
	T = FMath::Clamp(T, 0.0f, 1.0f);

	const float P1_y = Curve[1];
	const float P2_y = Curve[3];

	const float OneMinusT = 1.0f - T;
	const float TSquared = T * T;
	const float OneMinusTSquared = OneMinusT * OneMinusT;

	// Cubic Bezier 공식: y(t) = 3(1-t)^2 * t * P1_y + 3(1-t) * t^2 * P2_y + t^3
	// 시작점 (0, 0), 끝점 (1, 1) 고정
	return (3.0f * OneMinusTSquared * T * P1_y) +
		   (3.0f * OneMinusT * TSquared * P2_y) +
		   (TSquared * T);
}
