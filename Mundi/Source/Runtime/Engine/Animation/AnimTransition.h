#pragma once
#include "Name.h"
#include <functional>
#include "FAnimTransition.generated.h"

// Phase 2: Transition Rule
USTRUCT(DisplayName="애니메이션 트랜지션")
struct FAnimTransition
{
    GENERATED_REFLECTION_BODY()

    UPROPERTY(LuaReadWrite, EditAnywhere, Category="Transition")
    FName FromState;

    UPROPERTY(LuaReadWrite, EditAnywhere, Category="Transition")
    FName ToState;

    UPROPERTY(LuaReadWrite, EditAnywhere, Category="Transition")
    float BlendDuration = 0.3f;

    // Phase 1: Blend Curve (Bezier)
    // float[4] 배열: P1(BlendCurve[0], BlendCurve[1]), P2(BlendCurve[2], BlendCurve[3])
    // 시작점 (0, 0), 끝점 (1, 1) 고정
    // 기본값: Linear {0.0f, 0.0f, 1.0f, 1.0f}
    UPROPERTY(LuaReadWrite, EditAnywhere, Category="Transition")
    float BlendCurve[4] = { 0.0f, 0.0f, 1.0f, 1.0f };

    // 조건부 자동 전환용 (nullptr이면 수동 전환)
    // ⚠️ 리플렉션 제외: std::function은 직렬화 불가
    std::function<bool()> Condition;
};
