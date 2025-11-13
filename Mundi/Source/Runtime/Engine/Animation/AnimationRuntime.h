#pragma once
#include "AnimationTypes.h"

// 애니메이션 런타임 유틸리티 (발제 문서 예제)
class FAnimationRuntime
{
public:
	// 두 포즈 블렌딩 (팀원2가 사용할 핵심 함수)
	static void BlendTwoPosesTogether(
		const FPoseContext& PoseA,
		const FPoseContext& PoseB,
		float BlendAlpha,
		FPoseContext& OutPose);

	// 개별 트랜스폼 블렌딩
	static FTransform BlendTransforms(
		const FTransform& A,
		const FTransform& B,
		float Alpha);
};
