#pragma once
#include <Name.h>
#include <AnimState.h>


UCLASS(DisplayName="애니메이션 스테이트 머신", Description="상태 기반 애니메이션 전환 시스템")
class UAnimStateMachine : public UObject
{
public:
    GENERATED_REFLECTION_BODY()

    UPROPERTY(LuaReadWrite)
    FName CurrentState;

    UPROPERTY(LuaReadWrite)
    TMap<FName, FAnimState> States;

    void AddState(FName StateName, class UAnimSequence* Animation, bool bLoop = true, float PlayRate = 1.0f);
    void SetInitialState(FName StateName);

    FName GetCurrentState() const { return CurrentState; }
    class UAnimSequence* GetCurrentAnimation() const;
    bool IsTransitioning() const { return bIsTransitioning; }
    float GetTransitionAlpha() const { return TransitionAlpha; }

    void TransitionTo(FName NewState);

    void Update(float DeltaTime);
    void GetBlendedPose(float DeltaTime, struct FPoseContext& OutPose);

    virtual void ProcessState() {}

private:
    bool bIsTransitioning = false;
    FName FromState;
    FName ToState;
    float TransitionAlpha = 0.0f;
    float TransitionDuration = 0.3f;
    float TransitionElapsed = 0.0f;
    float CurrentTime = 0.0f;

    void StartTransition(FName From, FName To, float Duration);
    void UpdateTransition(float DeltaTime);
};