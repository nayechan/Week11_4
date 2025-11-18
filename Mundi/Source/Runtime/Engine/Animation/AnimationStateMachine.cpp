#include "pch.h"
#include "AnimationStateMachine.h"
#include "AnimInstance.h"
#include "AnimationRuntime.h"
#include "AnimationTypes.h"
#include "GlobalConsole.h"

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
    NewState.bLoop = bLoop;
    NewState.PlayRate = PlayRate;

    States.Add(StateName, NewState);
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

    // Node-Centric 아키텍처 + Phase Sync:
    // FromState: 현재 시간 계속 (예: 2.0초 → 2.033초 → ...)
    // ToState: FromState의 phase(진행률)를 유지하여 시작
    FAnimState* FromStatePtr = States.Find(From);
    FAnimState* ToStatePtr = States.Find(To);

    if (ToStatePtr)
    {
        // Phase 기반 동기화: FromState의 진행률(0.0~1.0)을 ToState에 적용
        // 예: Walk(2.0초) 1.0초 지점 → phase=0.5 → Run(1.5초)은 0.75초부터 시작
        if (FromStatePtr && FromStatePtr->Animation && ToStatePtr->Animation)
        {
            float fromLength = FromStatePtr->Animation->SequenceLength;
            float toLength = ToStatePtr->Animation->SequenceLength;

            if (fromLength > 0.0f && toLength > 0.0f)
            {
                // Normalized phase 계산
                float phase = FromStatePtr->InternalTime / fromLength;
                // Loop 중일 경우 phase가 1.0을 초과할 수 있으므로 fmod로 정규화
                phase = fmod(phase, 1.0f);
                ToStatePtr->InternalTime = phase * toLength;

                // Phase sync 로그 (간소화)
                UE_LOG("StateMachine: Phase sync - phase=%.2f, ToTime=%.2fs",
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
        // Transition 중: FromState와 ToState 모두 업데이트
        FAnimState* FromStatePtr = States.Find(FromState);
        FAnimState* ToStatePtr = States.Find(ToState);

        if (FromStatePtr)
        {
            FromStatePtr->PreviousInternalTime = FromStatePtr->InternalTime;
            FromStatePtr->InternalTime += DeltaTime * FromStatePtr->PlayRate;

            // Loop 처리 (UE5 패턴)
            if (FromStatePtr->bLoop && FromStatePtr->Animation)
            {
                float SequenceLength = FromStatePtr->Animation->SequenceLength;
                if (SequenceLength > 0.0f && FromStatePtr->InternalTime >= SequenceLength)
                {
                    FromStatePtr->InternalTime = fmod(FromStatePtr->InternalTime, SequenceLength);
                }
            }
        }

        if (ToStatePtr)
        {
            ToStatePtr->PreviousInternalTime = ToStatePtr->InternalTime;
            ToStatePtr->InternalTime += DeltaTime * ToStatePtr->PlayRate;

            // Loop 처리 (UE5 패턴)
            if (ToStatePtr->bLoop && ToStatePtr->Animation)
            {
                float SequenceLength = ToStatePtr->Animation->SequenceLength;
                if (SequenceLength > 0.0f && ToStatePtr->InternalTime >= SequenceLength)
                {
                    ToStatePtr->InternalTime = fmod(ToStatePtr->InternalTime, SequenceLength);
                }
            }
        }
    }
    else
    {
        // 일반: CurrentState만 업데이트
        FAnimState* CurrentStatePtr = States.Find(CurrentState);
        if (CurrentStatePtr)
        {
            CurrentStatePtr->PreviousInternalTime = CurrentStatePtr->InternalTime;
            CurrentStatePtr->InternalTime += DeltaTime * CurrentStatePtr->PlayRate;

            // Loop 처리 (UE5 패턴)
            if (CurrentStatePtr->bLoop && CurrentStatePtr->Animation)
            {
                float SequenceLength = CurrentStatePtr->Animation->SequenceLength;
                if (SequenceLength > 0.0f && CurrentStatePtr->InternalTime >= SequenceLength)
                {
                    CurrentStatePtr->InternalTime = fmod(CurrentStatePtr->InternalTime, SequenceLength);
                }
            }

            // 디버그: InternalTime 출력
            static float LogTimer = 0.0f;
            LogTimer += DeltaTime;
            if (LogTimer >= 0.5f) // 0.5초마다 로그
            {
                UE_LOG("StateMachine: State='%s', InternalTime=%.2fs, bLoop=%d, Animation=%p",
                    CurrentState.ToString().c_str(),
                    CurrentStatePtr->InternalTime,
                    CurrentStatePtr->bLoop ? 1 : 0,
                    CurrentStatePtr->Animation);
                LogTimer = 0.0f;
            }
        }
    }

    ProcessState();

    UpdateTransition(DeltaTime);

    // Phase 2: 자동 전환 체크 (Transition 중이 아닐 때만)
    if (!bIsTransitioning)
    {
        CheckAutoTransitions();
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

        if (FromStatePtr && ToStatePtr && FromStatePtr->Animation && ToStatePtr->Animation)
        {
            FPoseContext PoseA, PoseB;

            // 각 State가 자신의 InternalTime 사용
            FAnimExtractContext FromContext(FromStatePtr->InternalTime, FromStatePtr->bLoop);
            FAnimExtractContext ToContext(ToStatePtr->InternalTime, ToStatePtr->bLoop);

            FromStatePtr->Animation->GetAnimationPose(PoseA, FromContext);
            ToStatePtr->Animation->GetAnimationPose(PoseB, ToContext);

            // Phase 1: Blend Curve 적용
            // 선형 TransitionAlpha를 Bezier curve로 평가하여 부드러운 전환
            float CurvedAlpha = TransitionAlpha;
            FAnimTransition* CurrentTransition = FindTransition(FromState, ToState);
            if (CurrentTransition)
            {
                CurvedAlpha = FAnimationRuntime::EvaluateBezierCurve(CurrentTransition->BlendCurve, TransitionAlpha);
            }

            FAnimationRuntime::BlendTwoPosesTogether(
                PoseA,
                PoseB,
                CurvedAlpha,
                OutPose
            );

            // Transition 중: From과 To 애니메이션 모두에서 Notify 수집
            // 각 State의 독립적인 시간 범위 사용
            TArray<FAnimNotifyEvent> FromNotifies, ToNotifies;
            FromStatePtr->Animation->GetAnimNotifiesInRange(
                FromStatePtr->PreviousInternalTime,
                FromStatePtr->InternalTime,
                FromNotifies
            );
            ToStatePtr->Animation->GetAnimNotifiesInRange(
                ToStatePtr->PreviousInternalTime,
                ToStatePtr->InternalTime,
                ToNotifies
            );

            // 트리 누적 패턴: 수집한 Notify를 OutPose에 추가
            OutPose.AnimNotifies.Append(FromNotifies);
            OutPose.AnimNotifies.Append(ToNotifies);
        }
        else
        {
            OutPose.BoneTransforms.Empty();
        }
    }
    else
    {
        const FAnimState* StatePtr = States.Find(CurrentState);
        if (StatePtr && StatePtr->Animation)
        {
            FAnimExtractContext ExtractContext(StatePtr->InternalTime, StatePtr->bLoop);
            StatePtr->Animation->GetAnimationPose(OutPose, ExtractContext);

            // 현재 State 애니메이션에서 Notify 수집
            TArray<FAnimNotifyEvent> CurrentNotifies;
            StatePtr->Animation->GetAnimNotifiesInRange(
                StatePtr->PreviousInternalTime,
                StatePtr->InternalTime,
                CurrentNotifies
            );

            // 트리 누적 패턴: 수집한 Notify를 OutPose에 추가
            OutPose.AnimNotifies.Append(CurrentNotifies);
        }
        else
        {
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
