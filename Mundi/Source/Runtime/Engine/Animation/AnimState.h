#pragma once
#include "Name.h"
#include "AnimSequence.h"
#include <functional>
#include "FAnimState.generated.h"

USTRUCT(DisplayName="애니메이션 스테이트")
struct FAnimState
{
    GENERATED_REFLECTION_BODY()

    UPROPERTY(LuaReadWrite, EditAnywhere, Category="State")
    FName StateName;

    UPROPERTY(LuaReadWrite, EditAnywhere, Category="Animation")
    UAnimSequence* Animation;

    UPROPERTY(LuaReadWrite, EditAnywhere, Category="Playback")
    bool bLoop = true;

    UPROPERTY(LuaReadWrite, EditAnywhere, Category="Playback")
    float PlayRate = 1.0f;

    UPROPERTY(LuaReadWrite, EditAnywhere, Category="Blending")
    float BlendInTime = 0.3f;

    UPROPERTY(LuaReadWrite, EditAnywhere, Category="Blending")
    float BlendOutTime = 0.3f;
};

// Phase 2: Transition Rule
struct FAnimTransition
{
    FName FromState;
    FName ToState;
    float BlendDuration = 0.3f;
    std::function<bool()> Condition;  // 조건부 자동 전환용 (nullptr이면 수동 전환)
};
