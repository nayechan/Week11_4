#pragma once
#include "Name.h"
#include "AnimState.h"
#include "AnimTransition.h"

// Forward declaration (CharacterAnimInstance.h creates circular dependency)
class UAnimInstance;

UCLASS(DisplayName="애니메이션 스테이트 머신", Description="상태 기반 애니메이션 전환 시스템")
class UAnimStateMachine : public UObject
{
public:
    GENERATED_REFLECTION_BODY()

    UPROPERTY(LuaReadWrite)
    FName CurrentState;

    UPROPERTY(LuaReadWrite)
    TMap<FName, FAnimState> States;

    // Phase 2: Transition Rules
    UPROPERTY(LuaReadWrite)
    TArray<FAnimTransition> Transitions;

    // ========================================
    // 초기화 파이프라인
    // ========================================

    // StateMachine 초기화 (NativeSetup → LuaSetup 순서로 호출)
    UFUNCTION(LuaBind, DisplayName="Initialize")
    void Initialize();

    // TODO : Bind FAnimTransition / UAnimSequence (?)
    // TODO : SkeletalMeshComponent에서 사용 가능한 Animation 목록 가져오기
    //        - GetAvailableAnimSequences() 같은 함수 추가
    //        - SkeletalMesh에 포함된 AnimSequence들을 반환
    //        - Lua에서 목록을 보고 선택할 수 있도록
    // TODO : Animation 유효성 검증 기능 추가
    //        - AddState 시 Animation이 SkeletalMesh와 호환되는지 검증
    //        - Skeleton 호환성 체크 (본 구조가 일치하는지)

    UFUNCTION(LuaBind, DisplayName = "AddState")
    void AddState(FName StateName, class UAnimSequence* Animation, bool bLoop, float PlayRate);

    UFUNCTION(LuaBind, DisplayName = "SetInitialState")
    void SetInitialState(FName StateName);

    UFUNCTION(LuaBind, DisplayName = "GetCurrentState")
    FName GetCurrentState() const { return CurrentState; }

    UFUNCTION(LuaBind, DisplayName = "GetCurrentAnimation")
    class UAnimSequence* GetCurrentAnimation() const;

    UFUNCTION(LuaBind, DisplayName = "IsTransitioning")
    bool IsTransitioning() const { return bIsTransitioning; }

    UFUNCTION(LuaBind, DisplayName = "GetTransitionAlpha")
    float GetTransitionAlpha() const { return TransitionAlpha; }

    // Transition 중 활성 애니메이션
    UFUNCTION(LuaBind, DisplayName = "GetFromAnimation")
    class UAnimSequence* GetFromAnimation() const;

    UFUNCTION(LuaBind, DisplayName = "GetToAnimation")
    class UAnimSequence* GetToAnimation() const;

    // Phase 2: Transition APIs
    UFUNCTION(LuaBind, DisplayName = "AddTransition")
    void AddTransition(FName From, FName To, float BlendDuration);

    // Phase 1: AddTransition with Blend Curve (오버로드)
    UFUNCTION(LuaBind, DisplayName = "AddTransitionWithCurve")
    void AddTransition(FName From, FName To, float BlendDuration, float P1x, float P1y, float P2x, float P2y);

    // NOTE: AddTransitionWithCondition은 std::function 때문에 자동 바인딩 불가
    // Lua에서 조건을 확인하려면 수동으로 체크하거나 별도 래퍼 필요
    void AddTransitionWithCondition(FName From, FName To, float Blend, std::function<bool()> Condition);

    // NOTE: FAnimTransition* 반환은 자동 바인딩 어려움 (구조체 포인터)
    // 필요하면 수동 바인딩 또는 인덱스 기반 접근으로 변경
    FAnimTransition* FindTransition(FName From, FName To);

    void CheckAutoTransitions();

    UFUNCTION(LuaBind, DisplayName = "TransitionTo")
    void TransitionTo(FName NewState);

    // Update: Transition 처리 + 내부 시간 업데이트
    UFUNCTION(LuaBind, DisplayName = "Update")
    void Update(float DeltaTime);

    // ========================================
    // GetBlendedPose: 현재 상태의 포즈 추출 + Notify 수집
    // ========================================
    //
    // ⭐ 현재 구현: 단순 블렌딩 (Transition 중 FromState + ToState 2개만)
    //
    // TODO (미래 확장): 고급 블렌딩이 필요한 경우 Strategy Pattern 적용
    //
    // 확장 방법:
    // 1. IAnimBlendStrategy 인터페이스 생성
    //    - virtual void EvaluatePose(UAnimStateMachine* Machine, FPoseContext& OutPose) = 0;
    //
    // 2. 구체 전략 구현
    //    - USimpleBlendStrategy      : 현재 로직 (2개 블렌딩)
    //    - ULayeredBlendStrategy     : 계층적 블렌딩 (상체/하체 분리)
    //    - UAdditiveBlendStrategy    : Additive 블렌딩 (베이스 + 오프셋)
    //    - UBlendSpaceStrategy       : 파라미터 기반 블렌딩
    //
    // 3. StateMachine에 전략 멤버 추가
    //    - UPROPERTY(LuaReadWrite) IAnimBlendStrategy* BlendStrategy;
    //    - UFUNCTION(LuaBind) void SetBlendStrategy(IAnimBlendStrategy* Strategy);
    //
    // 4. GetBlendedPose에서 전략 위임
    //    - if (BlendStrategy) BlendStrategy->EvaluatePose(this, OutPose);
    //    - else DefaultBlendLogic(OutPose);  // Fallback
    //
    // 5. Lua에서 전략 설정 (Setup 시 또는 런타임)
    //    - local strategy = NewObject("ULayeredBlendStrategy", self)
    //    - self:SetBlendStrategy(strategy)
    //
    // 장점:
    // - 같은 UAnimStateMachine 클래스라도 인스턴스별 다른 블렌딩 가능
    // - 새 블렌딩 방식 추가 시 기존 코드 수정 불필요 (OCP)
    // - 런타임 전략 교체 가능 (예: 조준 모드 전환 시)
    // - Lua에서 설정만 하면 C++이 최적화된 블렌딩 수행
    //
    // 참고: Unreal Engine의 FAnimNode 시스템도 유사한 패턴 사용
    //
    // ========================================

    // C++ 버전 (참조)
    void GetBlendedPose(struct FPoseContext& OutPose);

    // Lua 버전 (포인터) - 자동 바인딩
    UFUNCTION(LuaBind, DisplayName="GetBlendedPose")
    void GetBlendedPose(struct FPoseContext* OutPose)
    {
        if (OutPose) GetBlendedPose(*OutPose);
    }

    // ========================================
    // 확장 포인트 (하위 클래스에서 오버라이드)
    // ========================================

    // C++ 네이티브 Setup (UAnimStateMachine 서브클래스용)
    virtual void NativeSetup() {}

    // Lua Setup (ULuaAnimStateMachine에서 구현)
    virtual void LuaSetup() {}

    // 매 Update마다 호출되는 콜백 (상태별 커스텀 로직)
    virtual void ProcessState() {}

    // World 접근 (Outer 체인을 통해)
    class UWorld* GetWorld() const;

private:
    bool bIsTransitioning = false;
    FName FromState;
    FName ToState;
    float TransitionAlpha = 0.0f;
    float TransitionDuration = 0.3f;
    float TransitionElapsed = 0.0f;

    // ⭐ Node-Centric 아키텍처 완성:
    // 각 State(FAnimState)가 자신의 InternalTime을 소유
    // StateMachine은 더 이상 전역 시간을 관리하지 않음

    void StartTransition(FName From, FName To, float Duration);
    void UpdateTransition(float DeltaTime);
};