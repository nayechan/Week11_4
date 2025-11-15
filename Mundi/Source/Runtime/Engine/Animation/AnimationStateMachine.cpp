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

    const FAnimState* FromStatePtr = States.Find(CurrentState);
    const FAnimState* ToStatePtr = States.Find(NewState);

    if (FromStatePtr && ToStatePtr)
    {
        BlendDuration = (FromStatePtr->BlendOutTime + ToStatePtr->BlendInTime) * 0.5f;
    }

    StartTransition(CurrentState, NewState, BlendDuration);
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
