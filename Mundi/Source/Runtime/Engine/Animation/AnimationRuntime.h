#pragma once
#include "AnimationTypes.h"

// 애니메이션 런타임 유틸리티 (발제 문서 예제)
class FAnimationRuntime
{
public:
	// 두 포즈 블렌딩
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

	/**
	 * @brief Bezier curve를 평가하여 보간된 Y 값 반환
	 * @param Curve float[4] 배열 (P1(Curve[0], Curve[1]), P2(Curve[2], Curve[3]))
	 * @param T 정규화된 시간 (0.0 ~ 1.0)
	 * @return 보간된 Y 값
	 *
	 * Cubic Bezier: 시작점 (0,0), 끝점 (1,1) 고정
	 * y(t) = 3(1-t)^2 * t * P1_y + 3(1-t) * t^2 * P2_y + t^3
	 */
	static float EvaluateBezierCurve(const float Curve[4], float T);
};
