#pragma once
#include "AnimNode.h"
#include "UAnimNode_BlendTwoWay.generated.h"

/**
 * @brief 두 포즈를 Alpha로 블렌딩하는 노드
 *
 * 기존 AnimationStateMachine의 Transition 로직을 일반화
 *
 * 주요 기능:
 * - 두 입력 노드의 포즈를 Alpha 값으로 블렌딩
 * - Alpha = 0.0 → 100% InputA
 * - Alpha = 1.0 → 100% InputB
 * - Bezier Curve 블렌딩 지원 (부드러운 전환)
 *
 * 사용 예시:
 *   auto* IdlePlayer = Instance->CreateNode<UAnimNode_SequencePlayer>();
 *   auto* WalkPlayer = Instance->CreateNode<UAnimNode_SequencePlayer>();
 *
 *   auto* Blender = Instance->CreateNode<UAnimNode_BlendTwoWay>();
 *   Blender->InputA = IdlePlayer;
 *   Blender->InputB = WalkPlayer;
 *   Blender->BlendAlpha = 0.5f;  // 50-50 블렌딩
 *
 *   Instance->RootNode = Blender;
 *
 * Composite Pattern:
 * - 자식 노드: InputA, InputB
 * - Update(): 양쪽 자식 모두 업데이트
 * - Evaluate(): 양쪽 포즈 추출 → 블렌딩 → Notify 합치기
 */
UCLASS(DisplayName="2-Way 블렌딩", Description="두 포즈를 Alpha로 블렌딩")
class UAnimNode_BlendTwoWay : public UAnimNode
{
public:
	GENERATED_REFLECTION_BODY()

	UAnimNode_BlendTwoWay() = default;
	virtual ~UAnimNode_BlendTwoWay() = default;

	// ========================================
	// 프로퍼티
	// ========================================

	/**
	 * @brief 첫 번째 입력 노드 (Alpha = 0.0일 때 100%)
	 *
	 * 주의:
	 * - nullptr일 경우 InputB만 사용
	 * - 순환 참조 금지 (자기 자신을 자식으로 설정 불가)
	 */
	UPROPERTY(LuaReadWrite, EditAnywhere, Category="[Blend]", Tooltip="첫 번째 입력 포즈")
	UAnimNode* InputA = nullptr;

	/**
	 * @brief 두 번째 입력 노드 (Alpha = 1.0일 때 100%)
	 *
	 * 주의:
	 * - nullptr일 경우 InputA만 사용
	 * - 순환 참조 금지
	 */
	UPROPERTY(LuaReadWrite, EditAnywhere, Category="[Blend]", Tooltip="두 번째 입력 포즈")
	UAnimNode* InputB = nullptr;

	/**
	 * @brief 블렌딩 알파 값 (0.0 ~ 1.0)
	 *
	 * - 0.0: 100% InputA, 0% InputB
	 * - 0.5: 50% InputA, 50% InputB
	 * - 1.0: 0% InputA, 100% InputB
	 *
	 * bUseBezierCurve가 true면 Bezier Curve로 평가됨
	 */
	UPROPERTY(LuaReadWrite, EditAnywhere, Category="[Blend]", Tooltip="블렌딩 알파 (0~1)", Range="0.0, 1.0")
	float BlendAlpha = 0.0f;

	/**
	 * @brief Bezier Curve 블렌딩 사용 여부
	 *
	 * - false: Linear 블렌딩 (BlendAlpha 그대로 사용)
	 * - true: Bezier Curve 블렌딩 (부드러운 전환)
	 *
	 * Bezier Curve는 시작/끝에서 가속/감속 효과를 줍니다.
	 */
	UPROPERTY(LuaReadWrite, EditAnywhere, Category="[Blend]", Tooltip="Bezier Curve 블렌딩 사용")
	bool bUseBezierCurve = false;

	/**
	 * @brief Bezier Curve 제어점 (P1, P2)
	 *
	 * Cubic Bezier Curve:
	 * - 시작점: (0, 0) 고정
	 * - 끝점: (1, 1) 고정
	 * - P1: (BezierCurve[0], BezierCurve[1])
	 * - P2: (BezierCurve[2], BezierCurve[3])
	 *
	 * 공식:
	 *   y(t) = 3(1-t)^2 * t * P1_y + 3(1-t) * t^2 * P2_y + t^3
	 *
	 * 기본값: {0.0, 0.0, 1.0, 1.0} (선형과 동일)
	 *
	 * 부드러운 전환 예시:
	 *   {0.25, 0.1, 0.75, 0.9} - Ease In/Out
	 *   {0.42, 0.0, 0.58, 1.0} - Ease In/Out (강함)
	 */
	UPROPERTY(LuaReadWrite, EditAnywhere, Category="[Blend]", Tooltip="Bezier Curve 제어점 [P1.x, P1.y, P2.x, P2.y]")
	float BezierCurve[4] = {0.0f, 0.0f, 1.0f, 1.0f};

	// ========================================
	// UAnimNode 인터페이스 구현
	// ========================================

	/**
	 * @brief 시간 진행: 양쪽 입력 노드 모두 업데이트
	 *
	 * Composite Pattern:
	 * - 자식 노드들의 Update() 호출
	 * - 블렌딩 중에는 양쪽 모두 시간이 진행됨
	 *
	 * 주의:
	 * - InputA와 InputB가 각자의 InternalTime을 가짐
	 * - 서로 다른 길이의 애니메이션도 블렌딩 가능
	 * - nullptr 체크 필수
	 */
	virtual void Update(float DeltaTime) override
	{
		if (InputA)
			InputA->Update(DeltaTime);

		if (InputB)
			InputB->Update(DeltaTime);
	}

	/**
	 * @brief 포즈 블렌딩 + Notify 합치기
	 *
	 * 로직:
	 * 1. InputA와 InputB 각각 Evaluate() 호출 → PoseA, PoseB
	 * 2. BlendAlpha를 Bezier Curve로 평가 (옵션)
	 * 3. FAnimationRuntime::BlendTwoPosesTogether(PoseA, PoseB, Alpha, OutPose)
	 * 4. 양쪽 Notify를 OutPose.AnimNotifies에 합치기 (트리 누적)
	 *
	 * Fallback:
	 * - InputA만 있으면 InputA 포즈만 반환
	 * - InputB만 있으면 InputB 포즈만 반환
	 * - 둘 다 없으면 RefPose
	 *
	 * 주의:
	 * - Notify 중복 가능성 (양쪽에서 같은 Notify 발생 시)
	 * - 필요시 중복 제거 로직 추가
	 */
	virtual void Evaluate(FPoseContext& OutPose) override;

	/**
	 * @brief 노드 이름 반환 (디버깅용)
	 */
	virtual FString GetNodeName() const override

	{
		return "BlendTwoWay";
	}
};
