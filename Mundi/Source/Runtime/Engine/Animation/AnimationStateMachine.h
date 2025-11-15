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

    // Phase 2: Transition Rules
    TArray<FAnimTransition> Transitions;

    void AddState(FName StateName, class UAnimSequence* Animation, bool bLoop = true, float PlayRate = 1.0f);
    void SetInitialState(FName StateName);

    FName GetCurrentState() const { return CurrentState; }
    class UAnimSequence* GetCurrentAnimation() const;
    bool IsTransitioning() const { return bIsTransitioning; }
    float GetTransitionAlpha() const { return TransitionAlpha; }

    // Transition 중 활성 애니메이션
    class UAnimSequence* GetFromAnimation() const;
    class UAnimSequence* GetToAnimation() const;

    // Phase 2: Transition APIs
    void AddTransition(FName From, FName To, float BlendDuration = 0.3f);
    void AddTransitionWithCondition(FName From, FName To, float Blend, std::function<bool()> Condition);
    FAnimTransition* FindTransition(FName From, FName To);
    void CheckAutoTransitions();

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