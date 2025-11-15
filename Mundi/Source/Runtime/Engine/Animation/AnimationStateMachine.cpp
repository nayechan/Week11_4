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
        CurrentTime = 0.0f;

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

    if (TransitionAlpha >= 1.0f)
    {
        CurrentState = ToState;
        bIsTransitioning = false;
        TransitionAlpha = 0.0f;
        CurrentTime = 0.0f;

        UE_LOG("StateMachine: Transition complete -> %s", CurrentState.ToString().c_str());
    }
}

void UAnimStateMachine::Update(float DeltaTime)
{
    CurrentTime += DeltaTime;

    ProcessState();

    UpdateTransition(DeltaTime);

    // Phase 2: 자동 전환 체크 (Transition 중이 아닐 때만)
    if (!bIsTransitioning)
    {
        CheckAutoTransitions();
    }
}

void UAnimStateMachine::GetBlendedPose(float DeltaTime, FPoseContext& OutPose)
{
    Update(DeltaTime);

    if (bIsTransitioning)
    {
        const FAnimState* FromStatePtr = States.Find(FromState);
        const FAnimState* ToStatePtr = States.Find(ToState);

        if (FromStatePtr && ToStatePtr && FromStatePtr->Animation && ToStatePtr->Animation)
        {
            FPoseContext PoseA, PoseB;
            FAnimExtractContext ExtractContext(CurrentTime, false);

            FromStatePtr->Animation->GetAnimationPose(PoseA, ExtractContext);
            ToStatePtr->Animation->GetAnimationPose(PoseB, ExtractContext);

            FAnimationRuntime::BlendTwoPosesTogether(
                PoseA,
                PoseB,
                TransitionAlpha,
                OutPose
            );
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
            FAnimExtractContext ExtractContext(CurrentTime, StatePtr->bLoop);
            StatePtr->Animation->GetAnimationPose(OutPose, ExtractContext);
        }
        else
        {
            OutPose.BoneTransforms.Empty();
        }
    }
}
