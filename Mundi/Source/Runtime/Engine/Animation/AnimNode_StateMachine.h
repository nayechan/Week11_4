#pragma once
#include "AnimNode.h"
#include "AnimationStateMachine.h"
#include "UAnimNode_StateMachine.generated.h"

/**
 * @brief UAnimStateMachine을 노드 시스템에 통합하는 Adapter
 *
 * Adapter Pattern:
 * - Target Interface: UAnimNode (Initialize, Update, Evaluate)
 * - Adaptee: UAnimStateMachine (Initialize, Update, GetBlendedPose)
 * - Adapter: UAnimNode_StateMachine
 *
 * 설계 원칙:
 * - Composition over Inheritance: StateMachine을 멤버로 소유
 * - Adapter Pattern: 기존 StateMachine 로직을 노드 인터페이스로 변환
 * - Delegation: 모든 실제 동작은 StateMachine에 위임
 *
 * 장점:
 * 1. 기존 UAnimStateMachine 코드 100% 재사용
 * 2. Transition, Blend, State 관리 로직 재구현 불필요
 * 3. 기존 사용자 코드 호환 (StateMachine API 직접 호출 가능)
 * 4. 노드 시스템과 완벽 통합 (AnimGraphInstance에서 사용 가능)
 *
 * 사용 예시 (AnimGraphInstance에서):
 *   class UMyCharacterAnimGraph : public UAnimGraphInstance
 *   {
 *       virtual void NativeInitializeAnimation() override
 *       {
 *           Super::NativeInitializeAnimation();
 *
 *           // 1. StateMachine 생성 및 설정
 *           auto* SM = NewObject<UAnimStateMachine>(this);
 *           SM->AddState("Idle", IdleAnim, true, 1.0f);
 *           SM->AddState("Walk", WalkAnim, true, 1.0f);
 *           SM->AddTransition("Idle", "Walk", 0.3f);
 *           SM->AddTransition("Walk", "Idle", 0.3f);
 *           SM->SetInitialState("Idle");
 *
 *           // 2. StateMachine Node 생성
 *           auto* SMNode = CreateNode<UAnimNode_StateMachine>();
 *           SMNode->StateMachine = SM;
 *
 *           // 3. RootNode로 설정
 *           RootNode = SMNode;
 *       }
 *
 *       virtual void NativeUpdateAnimation(float DeltaSeconds) override
 *       {
 *           Super::NativeUpdateAnimation(DeltaSeconds);
 *
 *           // StateMachine 조건 업데이트
 *           auto* SMNode = Cast<UAnimNode_StateMachine>(RootNode);
 *           if (SMNode && SMNode->StateMachine)
 *           {
 *               float Speed = calculate speed;
 *               if (Speed > 100.0f)
 *                   SMNode->StateMachine->TransitionTo("Walk");
 *               else
 *                   SMNode->StateMachine->TransitionTo("Idle");
 *           }
 *       }
 *   };
 *
 * 또는 Lua에서:
 *   function MyAnimGraph:LuaInitializeAnimation()
 *       local sm = NewObject("UAnimStateMachine", self)
 *       sm:AddState("Idle", self.IdleAnim, true, 1.0)
 *       sm:AddState("Walk", self.WalkAnim, true, 1.0)
 *       sm:AddTransition("Idle", "Walk", 0.3)
 *       sm:AddTransition("Walk", "Idle", 0.3)
 *       sm:SetInitialState("Idle")
 *
 *       local smNode = self:CreateNodeByName("UAnimNode_StateMachine")
 *       smNode.StateMachine = sm
 *       self.RootNode = smNode
 *   end
 *
 *   function MyAnimGraph:LuaUpdateAnimation(DeltaTime)
 *       local speed = self:CalculateSpeed()
 *       if speed > 100 then
 *           self.RootNode.StateMachine:TransitionTo("Walk")
 *       else
 *           self.RootNode.StateMachine:TransitionTo("Idle")
 *       end
 *   end
 */
UCLASS(DisplayName="스테이트 머신 노드", Description="UAnimStateMachine을 노드 시스템에 통합")
class UAnimNode_StateMachine : public UAnimNode
{
public:
	GENERATED_REFLECTION_BODY()

	UAnimNode_StateMachine() = default;
	virtual ~UAnimNode_StateMachine() = default;

	// ========================================
	// Adaptee (기존 StateMachine)
	// ========================================

	/**
	 * @brief Wrapped StateMachine (Composition)
	 *
	 * 설정 방법:
	 * - 외부에서 NewObject<UAnimStateMachine>()로 생성
	 * - AddState, AddTransition, SetInitialState 호출
	 * - 이 노드의 StateMachine 프로퍼티에 할당
	 *
	 * 생명주기:
	 * - UPROPERTY로 GC 관리됨 (자동 메모리 해제)
	 * - 이 노드가 삭제되면 StateMachine도 자동 삭제
	 *
	 * 사용자 접근:
	 * - C++: SMNode->StateMachine->TransitionTo("Walk");
	 * - Lua: smNode.StateMachine:TransitionTo("Walk")
	 *
	 * 주의:
	 * - nullptr이면 RefPose 반환
	 * - Initialize() 호출 전에 설정해야 함
	 */
	UPROPERTY(LuaReadWrite, EditAnywhere, Category="StateMachine", Tooltip="래핑할 StateMachine 인스턴스")
	UAnimStateMachine* StateMachine = nullptr;

	// ========================================
	// UAnimNode Interface (Adapter)
	// ========================================

	/**
	 * @brief 노드 초기화
	 *
	 * 동작:
	 * - StateMachine->Initialize() 호출
	 * - StateMachine은 인자가 없으므로, 외부에서 이미 설정되었다고 가정
	 *
	 * 주의:
	 * - StateMachine이 nullptr이면 경고 로그 출력
	 * - Initialize 호출 전에 StateMachine 설정 필요
	 */
	virtual void Initialize(const FSkeleton* InSkeleton, UAnimInstance* InOwner) override;

	/**
	 * @brief 시간 업데이트
	 *
	 * 동작:
	 * - StateMachine->Update(DeltaTime) 위임
	 * - Transition 처리 (FromState → ToState)
	 * - 각 State의 InternalTime 업데이트
	 *
	 * 주의:
	 * - StateMachine이 nullptr이면 아무것도 안 함
	 */
	virtual void Update(float DeltaTime) override;

	/**
	 * @brief 포즈 추출
	 *
	 * 동작:
	 * - StateMachine->GetBlendedPose(OutPose) 위임
	 * - 현재 상태 애니메이션 추출
	 * - Transition 중이면 FromState + ToState 블렌딩
	 * - OutPose.AnimNotifies에 Notify 수집
	 *
	 * Fallback:
	 * - StateMachine이 nullptr → RefPose 반환
	 * - Skeleton도 nullptr → OutPose 비움
	 */
	virtual void Evaluate(FPoseContext& OutPose) override;

	/**
	 * @brief 노드 이름 반환 (디버깅용)
	 *
	 * 형식:
	 * - StateMachine 있음: "StateMachine(Idle)" (현재 상태 표시)
	 * - StateMachine 없음: "StateMachine(None)"
	 *
	 * 용도:
	 * - 로그 출력 시 어떤 상태인지 확인
	 * - 에디터 디버거에서 노드 식별
	 */
	virtual FString GetNodeName() const override;
};
