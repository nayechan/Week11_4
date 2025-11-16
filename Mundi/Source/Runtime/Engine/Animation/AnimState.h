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

    // ⭐ Node-Centric 아키텍처: 각 State가 자신의 InternalTime 소유
    // AnimSingleNodeInstance가 InternalTime을 가지듯이, State도 독립적인 시간을 가짐
    // 이를 통해 Transition 중 FromState와 ToState가 서로 다른 시간대에서 재생 가능
    float InternalTime = 0.0f;           // 현재 재생 시간
    float PreviousInternalTime = 0.0f;   // 이전 프레임 시간 (Notify 범위 검사용)
};

// Phase 2: Transition Rule
struct FAnimTransition
{
    FName FromState;
    FName ToState;
    float BlendDuration = 0.3f;
    std::function<bool()> Condition;  // 조건부 자동 전환용 (nullptr이면 수동 전환)
};
