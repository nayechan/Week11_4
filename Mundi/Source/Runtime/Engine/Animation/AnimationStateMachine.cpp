#include "pch.h"
#include "AnimationStateMachine.h"
#include "AnimInstance.h"
#include "AnimationRuntime.h"
#include "AnimationTypes.h"
#include "GlobalConsole.h"
#include "AnimNode.h"

// ========================================
// 초기화 파이프라인
// ========================================

void UAnimStateMachine::Initialize()
{
    UE_LOG("UAnimStateMachine::Initialize - Called on class: %s", GetClass()->Name);

    // 1. C++ 네이티브 Setup (하위 클래스에서 오버라이드)
    NativeSetup();
    UE_LOG("UAnimStateMachine::Initialize - NativeSetup() complete");

    // 2. Lua Setup (ULuaAnimStateMachine에서 구현)
    LuaSetup();
    UE_LOG("UAnimStateMachine::Initialize - LuaSetup() complete");
}

void UAnimStateMachine::AddState(FName StateName, UAnimSequence* Animation, bool bLoop, float PlayRate)
{
    FAnimState NewState;
    NewState.StateName = StateName;
    NewState.Animation = Animation;
    NewState.Node = nullptr;  // Legacy 모드: Node 사용 안 함
    NewState.bLoop = bLoop;
    NewState.PlayRate = PlayRate;

    States.Add(StateName, NewState);
}

void UAnimStateMachine::AddState(FName StateName, UAnimNode* Node)
{
    FAnimState NewState;
    NewState.StateName = StateName;
    NewState.Animation = nullptr;  // Node 모드: Animation 사용 안 함
    NewState.Node = Node;          // Node 기반 State
    NewState.bLoop = true;         // Node 내부에서 관리되므로 기본값
    NewState.PlayRate = 1.0f;      // Node 내부에서 관리되므로 기본값

    States.Add(StateName, NewState);

    UE_LOG("StateMachine: Added node-based state '%s' (Node=%p)",
           StateName.ToString().c_str(), Node);
}

void UAnimStateMachine::SetInitialState(FName StateName)
{
    if (States.Contains(StateName))
    {
        CurrentState = StateName;
        bIsTransitioning = false;

        // Node-Centric: 해당 State의 시간 초기화
        FAnimState* State = States.Find(StateName);
        if (State)
        {
            State->InternalTime = 0.0f;
            State->PreviousInternalTime = 0.0f;
        }

        UE_LOG("StateMachine: Initial state set to %s", StateName.ToString().c_str());
    }
}

UAnimSequence* UAnimStateMachine::GetCurrentAnimation() const
{
    if (bIsTransitioning)
    {
        const FAnimState* State = States.Find(ToState);
        return State ? State->Animation : nullptr;
    }
    else
    {
        const FAnimState* State = States.Find(CurrentState);
        return State ? State->Animation : nullptr;
    }
}

UAnimSequence* UAnimStateMachine::GetFromAnimation() const
{
    const FAnimState* State = States.Find(FromState);
    return State ? State->Animation : nullptr;
}

UAnimSequence* UAnimStateMachine::GetToAnimation() const
{
    const FAnimState* State = States.Find(ToState);
    return State ? State->Animation : nullptr;
}

void UAnimStateMachine::TransitionTo(FName NewState)
{
    if (!States.Contains(NewState))
    {
        UE_LOG("StateMachine: State '%s' does not exist!", NewState.ToString().c_str());
        return;
    }

    if (CurrentState == NewState)
    {
        return;
    }

    // 이미 전환 중이면 무시 (UE5 패턴)
    if (bIsTransitioning)
    {
        return;
    }

    float BlendDuration = 0.3f;

    // Phase 2: 먼저 Transition 배열에서 찾기
    FAnimTransition* Trans = FindTransition(CurrentState, NewState);
    if (Trans)
    {
        BlendDuration = Trans->BlendDuration;
    }
    else
    {
        // Fallback: State의 BlendIn/Out 사용 (Phase 1 방식)
        const FAnimState* FromStatePtr = States.Find(CurrentState);
        const FAnimState* ToStatePtr = States.Find(NewState);
        if (FromStatePtr && ToStatePtr)
        {
            BlendDuration = (FromStatePtr->BlendOutTime + ToStatePtr->BlendInTime) * 0.5f;
        }
    }

    StartTransition(CurrentState, NewState, BlendDuration);
}

void UAnimStateMachine::AddTransition(FName From, FName To, float BlendDuration)
{
    FAnimTransition Trans;
    Trans.FromState = From;
    Trans.ToState = To;
    Trans.BlendDuration = BlendDuration;
    Trans.Condition = nullptr;
    Transitions.Add(Trans);
}

void UAnimStateMachine::AddTransition(FName From, FName To, float BlendDuration, float P1x, float P1y, float P2x, float P2y)
{
    FAnimTransition Trans;
    Trans.FromState = From;
    Trans.ToState = To;
    Trans.BlendDuration = BlendDuration;
    Trans.BlendCurve[0] = P1x;
    Trans.BlendCurve[1] = P1y;
    Trans.BlendCurve[2] = P2x;
    Trans.BlendCurve[3] = P2y;
    Trans.Condition = nullptr;
    Transitions.Add(Trans);
}

void UAnimStateMachine::AddTransitionWithCondition(FName From, FName To, float Blend, std::function<bool()> Condition)
{
    FAnimTransition Trans;
    Trans.FromState = From;
    Trans.ToState = To;
    Trans.BlendDuration = Blend;
    Trans.Condition = Condition;
    Transitions.Add(Trans);
}

FAnimTransition* UAnimStateMachine::FindTransition(FName From, FName To)
{
    for (auto& Trans : Transitions)
    {
        if (Trans.FromState == From && Trans.ToState == To)
            return &Trans;
    }
    return nullptr;
}

void UAnimStateMachine::CheckAutoTransitions()
{
    for (auto& Trans : Transitions)
    {
        if (Trans.FromState == CurrentState && Trans.Condition)
        {
            if (Trans.Condition())
            {
                TransitionTo(Trans.ToState);
                break;  // 첫 번째 만족하는 전환만
            }
        }
    }
}

void UAnimStateMachine::StartTransition(FName From, FName To, float Duration)
{
    bIsTransitioning = true;
    FromState = From;
    ToState = To;
    TransitionDuration = Duration;
    TransitionElapsed = 0.0f;
    TransitionAlpha = 0.0f;

    // ========================================
    // Phase Synchronization 초기화 (Weight-based Leader Selection)
    // ========================================
    // - ToState를 FromState의 정규화된 시간으로 초기화
    // - Update()에서 매 프레임 TransitionAlpha 기준으로 Leader 교체
    //   → Alpha < 0.5: FromState가 Leader
    //   → Alpha >= 0.5: ToState가 Leader (주도권 교체!)
    FAnimState* FromStatePtr = States.Find(From);
    FAnimState* ToStatePtr = States.Find(To);

    if (ToStatePtr)
    {
        // ToState 초기 시간 설정: FromState의 phase로 시작
        if (FromStatePtr && FromStatePtr->Animation && ToStatePtr->Animation)
        {
            float fromLength = FromStatePtr->Animation->SequenceLength;
            float toLength = ToStatePtr->Animation->SequenceLength;

            if (fromLength > 0.0f && toLength > 0.0f)
            {
                // Normalized phase 계산 (0.0~1.0)
                float phase = FromStatePtr->InternalTime / fromLength;
                phase = fmod(phase, 1.0f); // Loop 중일 경우 정규화

                // ToState 초기 시간 = FromState의 정규화된 시간
                ToStatePtr->InternalTime = phase * toLength;

                UE_LOG("StateMachine: Phase sync init - phase=%.2f, ToTime=%.2fs",
                       phase, ToStatePtr->InternalTime);
            }
            else
            {
                ToStatePtr->InternalTime = 0.0f;
            }
        }
        else
        {
            ToStatePtr->InternalTime = 0.0f;
        }

        // PreviousInternalTime도 동일하게 설정 (첫 프레임에 Notify 중복 방지)
        ToStatePtr->PreviousInternalTime = ToStatePtr->InternalTime;
    }

    UE_LOG("StateMachine: Transition %s -> %s (%.2fs)",
           From.ToString().c_str(),
           To.ToString().c_str(),
           Duration);
}

void UAnimStateMachine::UpdateTransition(float DeltaTime)
{
    if (!bIsTransitioning)
        return;

    TransitionElapsed += DeltaTime;

    if (TransitionDuration > 0.0f)
    {
        TransitionAlpha = FMath::Clamp(TransitionElapsed / TransitionDuration, 0.0f, 1.0f);
    }
    else
    {
        TransitionAlpha = 1.0f;
    }

    // 디버그: Transition 진행 상황
    // UE_LOG("StateMachine: Transition %s->%s Alpha=%.2f (%.2f/%.2f)",
    //        FromState.ToString().c_str(), ToState.ToString().c_str(),
    //        TransitionAlpha, TransitionElapsed, TransitionDuration);

    if (TransitionAlpha >= 1.0f)
    {
        CurrentState = ToState;
        bIsTransitioning = false;
        TransitionAlpha = 0.0f;

        // Node-Centric: 각 State가 자신의 시간을 관리하므로 추가 작업 불필요
        // ToState는 이미 0초부터 시작하여 현재 시간까지 진행 중

        UE_LOG("StateMachine: Transition complete -> %s", CurrentState.ToString().c_str());
    }
}

void UAnimStateMachine::Update(float DeltaTime)
{
    // Node-Centric 아키텍처: 각 활성 State의 시간 업데이트

    if (bIsTransitioning)
    {
        // ========================================
        // Transition 중: Node/Animation 모드 자동 판별
        // ========================================
        FAnimState* FromStatePtr = States.Find(FromState);
        FAnimState* ToStatePtr = States.Find(ToState);

        // Node 모드 체크: 하나라도 Node면 Node 모드로 처리
        bool bIsNodeMode = (FromStatePtr && FromStatePtr->Node) || (ToStatePtr && ToStatePtr->Node);

        if (bIsNodeMode)
        {
            // ========================================
            // Node 모드: 단순 업데이트 (Node 내부에서 Phase Sync 처리)
            // ========================================
            UpdateState(FromStatePtr, DeltaTime);
            UpdateState(ToStatePtr, DeltaTime);
            return;
        }

        // ========================================
        // Animation 모드: Weight-based Leader Selection Phase Sync
        // ========================================

        if (!FromStatePtr || !ToStatePtr || !FromStatePtr->Animation || !ToStatePtr->Animation)
        {
            // 유효하지 않으면 독립 업데이트로 폴백
            if (FromStatePtr)
            {
                FromStatePtr->PreviousInternalTime = FromStatePtr->InternalTime;
                FromStatePtr->InternalTime += DeltaTime * FromStatePtr->PlayRate;

                if (FromStatePtr->bLoop && FromStatePtr->Animation)
                {
                    float SequenceLength = FromStatePtr->Animation->SequenceLength;
                    if (SequenceLength > 0.0f && FromStatePtr->InternalTime >= SequenceLength)
                        FromStatePtr->InternalTime = fmod(FromStatePtr->InternalTime, SequenceLength);
                }
            }

            if (ToStatePtr)
            {
                ToStatePtr->PreviousInternalTime = ToStatePtr->InternalTime;
                ToStatePtr->InternalTime += DeltaTime * ToStatePtr->PlayRate;

                if (ToStatePtr->bLoop && ToStatePtr->Animation)
                {
                    float SequenceLength = ToStatePtr->Animation->SequenceLength;
                    if (SequenceLength > 0.0f && ToStatePtr->InternalTime >= SequenceLength)
                        ToStatePtr->InternalTime = fmod(ToStatePtr->InternalTime, SequenceLength);
                }
            }
            return;
        }

        const float FromLength = FromStatePtr->Animation->SequenceLength;
        const float ToLength = ToStatePtr->Animation->SequenceLength;

        if (FromLength <= 0.0f || ToLength <= 0.0f)
            return;

        // ========================================
        // Weight-based Leader Selection (Unreal Engine 스타일)
        // ========================================
        // TransitionAlpha < 0.5 → FromState 가중치 높음 (1-Alpha > 0.5)
        // TransitionAlpha >= 0.5 → ToState 가중치 높음 (Alpha >= 0.5)

        if (TransitionAlpha < 0.5f)
        {
            // ========================================
            // Case 1: FromState가 Leader (전환 초기 ~ 중반)
            // ========================================

            // Step 1: Leader(FromState) 정규화된 시간 계산
            float LeaderNormalizedTime = FromStatePtr->InternalTime / FromLength;

            // Step 2: PreviousTime 저장
            FromStatePtr->PreviousInternalTime = FromStatePtr->InternalTime;
            ToStatePtr->PreviousInternalTime = ToStatePtr->InternalTime;

            // Step 3: Leader 시간 진행 (normalized time 기준)
            LeaderNormalizedTime += (DeltaTime * FromStatePtr->PlayRate) / FromLength;

            // Step 4: 루핑/클램핑 (normalized time 기준)
            if (FromStatePtr->bLoop)
            {
                LeaderNormalizedTime = fmod(LeaderNormalizedTime, 1.0f);
            }
            else
            {
                LeaderNormalizedTime = FMath::Min(LeaderNormalizedTime, 1.0f);
            }

            // Step 5: Leader의 절대 시간 업데이트
            FromStatePtr->InternalTime = LeaderNormalizedTime * FromLength;

            // Step 6: Follower(ToState)를 Leader의 NormalizedTime에 동기화
            ToStatePtr->InternalTime = LeaderNormalizedTime * ToLength;
        }
        else
        {
            // ========================================
            // Case 2: ToState가 Leader (전환 중반 ~ 완료)
            // ========================================

            // Step 1: Leader(ToState) 정규화된 시간 계산
            float LeaderNormalizedTime = ToStatePtr->InternalTime / ToLength;

            // Step 2: PreviousTime 저장
            FromStatePtr->PreviousInternalTime = FromStatePtr->InternalTime;
            ToStatePtr->PreviousInternalTime = ToStatePtr->InternalTime;

            // Step 3: Leader 시간 진행 (normalized time 기준)
            LeaderNormalizedTime += (DeltaTime * ToStatePtr->PlayRate) / ToLength;

            // Step 4: 루핑/클램핑 (normalized time 기준)
            if (ToStatePtr->bLoop)
            {
                LeaderNormalizedTime = fmod(LeaderNormalizedTime, 1.0f);
            }
            else
            {
                LeaderNormalizedTime = FMath::Min(LeaderNormalizedTime, 1.0f);
            }

            // Step 5: Leader의 절대 시간 업데이트
            ToStatePtr->InternalTime = LeaderNormalizedTime * ToLength;

            // Step 6: Follower(FromState)를 Leader의 NormalizedTime에 동기화
            FromStatePtr->InternalTime = LeaderNormalizedTime * FromLength;
        }
    }
    else
    {
        // 일반: CurrentState만 업데이트 (Node/Animation 자동 판별)
        FAnimState* CurrentStatePtr = States.Find(CurrentState);
        UpdateState(CurrentStatePtr, DeltaTime);
    }

    ProcessState();

    UpdateTransition(DeltaTime);

    // Phase 2: 자동 전환 체크 (Transition 중이 아닐 때만)
    if (!bIsTransitioning)
    {
        CheckAutoTransitions();
    }
}

bool UAnimStateMachine::ExtractStatePose(const FAnimState* State, FPoseContext& OutPose)
{
    if (!State)
        return false;

    // ========================================
    // 우선순위 1: Node 기반 State
    // ========================================
    if (State->Node)
    {
        // Node->Evaluate()로 포즈 추출
        State->Node->Evaluate(OutPose);
        return true;
    }

    // ========================================
    // 우선순위 2: Animation 기반 State (Legacy)
    // ========================================
    if (State->Animation)
    {
        // Animation->GetAnimationPose()로 포즈 추출
        FAnimExtractContext ExtractContext(State->InternalTime, State->bLoop);
        State->Animation->GetAnimationPose(OutPose, ExtractContext);

        // Notify 수집 (Animation 모드만)
        TArray<FAnimNotifyEvent> Notifies;
        State->Animation->GetAnimNotifiesInRange(
            State->PreviousInternalTime,
            State->InternalTime,
            Notifies
        );
        OutPose.AnimNotifies.Append(Notifies);

        return true;
    }

    // ========================================
    // 둘 다 없으면 실패
    // ========================================
    return false;
}

void UAnimStateMachine::UpdateState(FAnimState* State, float DeltaTime)
{
    if (!State)
        return;

    // ========================================
    // 우선순위 1: Node 기반 State
    // ========================================
    if (State->Node)
    {
        // Node 내부에서 시간 관리
        State->Node->Update(DeltaTime);
        return;
    }

    // ========================================
    // 우선순위 2: Animation 기반 State (Legacy)
    // ========================================
    if (State->Animation)
    {
        // StateMachine이 직접 InternalTime 관리
        State->PreviousInternalTime = State->InternalTime;
        State->InternalTime += DeltaTime * State->PlayRate;

        // Loop 처리
        if (State->bLoop)
        {
            float SequenceLength = State->Animation->SequenceLength;
            if (SequenceLength > 0.0f && State->InternalTime >= SequenceLength)
                State->InternalTime = fmod(State->InternalTime, SequenceLength);
        }
    }
}

void UAnimStateMachine::GetBlendedPose(FPoseContext& OutPose)
{
    // ========================================
    // 현재 구현: 단순 블렌딩
    // ========================================
    //
    // 이 함수는 2가지 경우만 처리:
    // 1. Transition 중: FromState + ToState 선형 블렌딩
    // 2. 일반: CurrentState만 재생
    //
    // TODO (미래 확장):
    // 고급 블렌딩 (계층적/Additive/BlendSpace)이 필요하면 Strategy Pattern 적용
    // - AnimationStateMachine.h의 GetBlendedPose 주석 참고
    // - IAnimBlendStrategy::EvaluatePose()로 위임
    //
    // ========================================

    // Node-Centric 아키텍처:
    // 1. Update()에서 각 State의 InternalTime이 업데이트됨
    // 2. 각 State의 InternalTime 기준으로 포즈 추출
    // 3. 각 State의 PreviousInternalTime ~ InternalTime 범위의 Notify 수집
    // 4. OutPose.AnimNotifies에 추가 (트리 누적 패턴)

    if (bIsTransitioning)
    {
        const FAnimState* FromStatePtr = States.Find(FromState);
        const FAnimState* ToStatePtr = States.Find(ToState);

        // Node/Animation 자동 판별로 포즈 추출
        FPoseContext PoseA, PoseB;
        bool bFromValid = ExtractStatePose(FromStatePtr, PoseA);
        bool bToValid = ExtractStatePose(ToStatePtr, PoseB);

        if (bFromValid && bToValid)
        {
            // Phase 1: Blend Curve 적용
            // 선형 TransitionAlpha를 Bezier curve로 평가하여 부드러운 전환
            float CurvedAlpha = TransitionAlpha;
            FAnimTransition* CurrentTransition = FindTransition(FromState, ToState);
            if (CurrentTransition)
            {
                CurvedAlpha = FAnimationRuntime::EvaluateBezierCurve(CurrentTransition->BlendCurve, TransitionAlpha);
            }

            // 두 포즈 블렌딩
            FAnimationRuntime::BlendTwoPosesTogether(
                PoseA,
                PoseB,
                CurvedAlpha,
                OutPose
            );

            // ExtractStatePose가 이미 Notify를 OutPose에 추가했으므로
            // 여기서는 추가 작업 불필요
        }
        else if (bFromValid)
        {
            // ToState가 유효하지 않으면 FromState만 사용
            OutPose = PoseA;
        }
        else if (bToValid)
        {
            // FromState가 유효하지 않으면 ToState만 사용
            OutPose = PoseB;
        }
        else
        {
            // 둘 다 유효하지 않으면 빈 포즈
            OutPose.BoneTransforms.Empty();
        }
    }
    else
    {
        // 일반 상태: CurrentState만 재생
        const FAnimState* StatePtr = States.Find(CurrentState);

        // Node/Animation 자동 판별로 포즈 추출
        if (!ExtractStatePose(StatePtr, OutPose))
        {
            // 유효한 State가 없으면 빈 포즈
            OutPose.BoneTransforms.Empty();
        }
    }
}

UWorld* UAnimStateMachine::GetWorld() const
{
    // Outer 체인을 통해 World 가져오기
    // StateMachine -> AnimInstance -> SkeletalMeshComponent -> Actor -> World
    UAnimInstance* AnimInst = Cast<UAnimInstance>(GetOuter());
    return AnimInst ? AnimInst->GetWorld() : nullptr;
}
