#include "pch.h"
#include "AnimationStateMachine.h"
#include "AnimationRuntime.h"
#include "AnimationTypes.h"
#include "GlobalConsole.h"

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

    // Node-Centric 아키텍처:
    // FromState: 현재 시간 계속 (예: 2.0초 → 2.033초 → ...)
    // ToState: 0초부터 시작 (0.0 → 0.033 → 0.066 → ...)
    FAnimState* ToStatePtr = States.Find(To);
    if (ToStatePtr)
    {
        ToStatePtr->InternalTime = 0.0f;
        ToStatePtr->PreviousInternalTime = 0.0f;
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
        }

        if (ToStatePtr)
        {
            ToStatePtr->PreviousInternalTime = ToStatePtr->InternalTime;
            ToStatePtr->InternalTime += DeltaTime * ToStatePtr->PlayRate;
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

            FAnimationRuntime::BlendTwoPosesTogether(
                PoseA,
                PoseB,
                TransitionAlpha,
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
