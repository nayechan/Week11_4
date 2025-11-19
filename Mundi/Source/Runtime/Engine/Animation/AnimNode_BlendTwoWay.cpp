#include "pch.h"
#include "AnimNode_BlendTwoWay.h"
#include "AnimationRuntime.h"
#include "VertexData.h"

// ========================================
// UAnimNode_BlendTwoWay 구현
// ========================================

void UAnimNode_BlendTwoWay::Evaluate(FPoseContext& OutPose)
{
	// ========================================
	// Fallback 처리
	// ========================================

	// 둘 다 없으면 RefPose
	if (!InputA && !InputB)
	{
		OutPose.SetNumBones(Skeleton ? Skeleton->Bones.Num() : 0);
		return;
	}

	// InputA만 있으면 InputA 포즈만 반환
	if (InputA && !InputB)
	{
		InputA->Evaluate(OutPose);
		return;
	}

	// InputB만 있으면 InputB 포즈만 반환
	if (!InputA && InputB)
	{
		InputB->Evaluate(OutPose);
		return;
	}

	// ========================================
	// 정상 블렌딩: InputA + InputB
	// ========================================

	// 1. 양쪽 포즈 추출
	FPoseContext PoseA, PoseB;
	InputA->Evaluate(PoseA);
	InputB->Evaluate(PoseB);

	// 2. Bezier Curve 평가 (옵션)
	float FinalAlpha = BlendAlpha;
	if (bUseBezierCurve)
	{
		FinalAlpha = FAnimationRuntime::EvaluateBezierCurve(BezierCurve, BlendAlpha);
	}

	// 3. 블렌딩 (AnimationStateMachine::GetBlendedPose 로직 참고)
	FAnimationRuntime::BlendTwoPosesTogether(
		PoseA,
		PoseB,
		FinalAlpha,
		OutPose
	);

	// 4. Notify 합치기 (트리 누적 패턴)
	// 주의: 양쪽에서 같은 Notify가 발생할 수 있음 (중복 가능)
	// 필요시 중복 제거 로직 추가 가능
	OutPose.AnimNotifies.Append(PoseA.AnimNotifies);
	OutPose.AnimNotifies.Append(PoseB.AnimNotifies);
}
