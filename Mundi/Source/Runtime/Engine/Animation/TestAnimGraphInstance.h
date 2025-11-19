#pragma once
#include "AnimGraphInstance.h"
#include "AnimNode_StateMachine.h"
#include "UTestAnimGraphInstance.generated.h"

/**
 * @brief CharacterAnimInstance와 동일한 기능을 노드 시스템으로 구현한 테스트 클래스
 *
 * 목적:
 * - 기존 CharacterAnimInstance의 동작을 노드 시스템으로 재현
 * - 통합 테스트: 노드 시스템이 기존 시스템과 동일하게 동작하는지 검증
 * - 회귀 테스트: 같은 입력(Speed)으로 같은 출력(Pose) 생성 확인
 *
 * 차이점 (노드 시스템):
 * - 부모 클래스: UAnimInstance → UAnimGraphInstance
 * - StateMachine 관리: 직접 소유 → 노드로 래핑 (UAnimNode_StateMachine)
 * - Update/GetPose: 명시적 호출 → RootNode 자동 위임
 *
 * 동일한 점 (게임 로직):
 * - 애니메이션 로드 방식 (ResourceManager)
 * - State 구성 (Idle, Walk, Run)
 * - Transition 규칙 (Speed 기반)
 * - 게임 로직 (UpdateMovementVariables, UpdateLowerBodyStateMachine)
 *
 * 사용 방법 (SkeletalMeshComponent):
 *   AnimationMode = EAnimationMode::AnimationNative;
 *   AnimClass = UTestAnimGraphInstance::StaticClass();
 *
 * 비교 테스트:
 * - CharacterAnimInstance와 TestAnimGraphInstance를 동일한 캐릭터에 교체하며 테스트
 * - 동일한 Speed 값에서 동일한 상태 전환 발생 확인
 * - 포즈 블렌딩 품질 비교 (육안 확인)
 *
 * 확장 예시:
 * - 여러 StateMachine 추가 (상체, 표정 등)
 * - 복잡한 노드 그래프 (레이어 블렌딩, IK 등)
 */
UCLASS(DisplayName="테스트 애니메이션 그래프 인스턴스", Description="CharacterAnimInstance의 노드 버전 (통합 테스트용)")
class UTestAnimGraphInstance : public UAnimGraphInstance
{
public:
	GENERATED_REFLECTION_BODY()

	UTestAnimGraphInstance() = default;
	virtual ~UTestAnimGraphInstance() = default;

	// ========================================
	// 애니메이션 시퀀스들 (CharacterAnimInstance와 동일)
	// ========================================

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Animations", Tooltip="Idle 상태 애니메이션")
	class UAnimSequence* IdleAnimation = nullptr;

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Animations", Tooltip="Walk 상태 애니메이션")
	class UAnimSequence* WalkAnimation = nullptr;

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Animations", Tooltip="Run 상태 애니메이션")
	class UAnimSequence* RunAnimation = nullptr;

	// ========================================
	// 게임플레이 변수들 (CharacterAnimInstance와 동일)
	// ========================================

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Movement")
	float Speed = 0.0f;

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Movement")
	bool bIsInAir = false;

	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Combat")
	bool bIsCombatMode = false;

	// ========================================
	// 노드 참조 (노드 시스템 전용)
	// ========================================

	/**
	 * @brief 하체 로코모션 StateMachine 노드
	 *
	 * 역할:
	 * - Idle/Walk/Run 상태 관리
	 * - Speed 기반 자동 전환
	 *
	 * 위치:
	 * - 현재: RootNode로 직접 사용 (단순 구조)
	 * - 확장: RootNode의 자식으로 사용 가능
	 *
	 * 확장 예시:
	 *   // 복잡한 구조 (상하체 분리)
	 *   RootNode = BlendTwoWay {
	 *       InputA = UpperBodyStateMachine,
	 *       InputB = LowerBodyStateMachine  ← 여기에 위치
	 *   };
	 *
	 * 생명주기:
	 * - NativeInitializeAnimation()에서 CreateNode<>()로 생성
	 * - UPROPERTY로 GC 관리 (자동 메모리 해제)
	 *
	 * 접근:
	 * - UpdateLowerBodyStateMachine()에서 사용
	 * - LowerBodyStateMachine->StateMachine->TransitionTo(...)
	 */
	UPROPERTY()
	UAnimNode_StateMachine* LowerBodyStateMachine = nullptr;

	// 확장 예시 (주석 처리):
	// UPROPERTY()
	// UAnimNode_StateMachine* UpperBodyStateMachine = nullptr;
	//
	// UPROPERTY()
	// UAnimNode_StateMachine* FacialStateMachine = nullptr;

	// ========================================
	// UAnimGraphInstance 오버라이드
	// ========================================

	/**
	 * @brief 초기화: 애니메이션 로드 + 노드 그래프 구성
	 *
	 * 동작:
	 * 1. Super::NativeInitializeAnimation() - 부모 초기화
	 * 2. LoadAnimations() - ResourceManager로 애니메이션 로드
	 * 3. CreateLowerBodyStateMachine() - StateMachine 생성 및 설정
	 * 4. CreateNode<UAnimNode_StateMachine>() - StateMachine을 노드로 래핑
	 * 5. RootNode 설정 (현재는 단순하게 LowerBodyStateMachine)
	 *
	 * CharacterAnimInstance와의 차이:
	 * - StateMachine을 직접 소유 → CreateNode<>()로 노드 생성 후 래핑
	 * - 노드 그래프 구성 가능 (확장성)
	 */
	virtual void NativeInitializeAnimation() override;

	/**
	 * @brief 업데이트: 게임 로직 + StateMachine 조건 체크
	 *
	 * 동작:
	 * 1. Super::NativeUpdateAnimation() - RootNode->Update() 자동 호출됨
	 * 2. UpdateMovementVariables() - Speed 계산
	 * 3. UpdateLowerBodyStateMachine() - TransitionTo() 호출
	 *
	 * CharacterAnimInstance와의 차이:
	 * - Super 호출 시 RootNode->Update() 자동 실행
	 * - 명시적 StateMachine->Update() 호출 불필요
	 *
	 * NOTE: UpdateLowerBodyStateMachine()는 프레임워크 메서드가 아님
	 *       개발자가 코드 정리를 위해 만든 헬퍼 함수 (선택사항)
	 */
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// ========================================
	// 헬퍼 함수들 (개발자가 자유롭게 작성)
	// ========================================

private:
	/**
	 * @brief 애니메이션 로드 (ResourceManager)
	 *
	 * 동작:
	 * - IdleAnimation, WalkAnimation, RunAnimation 로드
	 * - 로드 실패 시 경고 로그
	 *
	 * NOTE: CharacterAnimInstance와 동일한 로직
	 */
	void LoadAnimations();

	/**
	 * @brief 하체 StateMachine 생성 및 설정
	 *
	 * 동작:
	 * - NewObject<UAnimStateMachine>() 생성
	 * - AddState("Idle", "Walk", "Run")
	 * - AddTransition (모든 상태 간 전환 가능)
	 * - SetInitialState("Idle")
	 *
	 * NOTE: CharacterAnimInstance와 동일한 로직
	 *
	 * @return 설정된 StateMachine 포인터
	 */
	UAnimStateMachine* CreateLowerBodyStateMachine();

	/**
	 * @brief 이동 변수 업데이트
	 *
	 * 동작:
	 * - OwnerComponent에서 Actor 가져오기
	 * - Velocity 계산 (현재는 테스트용 사인파)
	 * - Speed 업데이트
	 *
	 * NOTE: CharacterAnimInstance와 동일한 로직
	 *       게임플레이 개발자가 자유롭게 작성한 헬퍼 (선택사항)
	 */
	void UpdateMovementVariables();

	/**
	 * @brief 하체 StateMachine 상태 전환 조건 체크
	 *
	 * 동작:
	 * - Speed >= 300.0f → Run
	 * - Speed >= 0.1f → Walk
	 * - Speed < 0.1f → Idle
	 *
	 * CharacterAnimInstance와의 차이:
	 * - StateMachine 직접 접근 → LowerBodyStateMachine->StateMachine 접근
	 *
	 * NOTE: CharacterAnimInstance와 동일한 전환 로직
	 *       게임플레이 개발자가 자유롭게 작성한 헬퍼 (선택사항)
	 */
	void UpdateLowerBodyStateMachine();
};
