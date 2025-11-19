#pragma once
#include "Name.h"
#include "AnimSequence.h"
#include "FAnimState.generated.h"

// 전방 선언
class UAnimNode;

USTRUCT(DisplayName="애니메이션 스테이트")
struct FAnimState
{
    GENERATED_REFLECTION_BODY()

    // 기본 생성자: 모든 포인터를 nullptr로 초기화
    FAnimState()
        : StateName()
        , Animation(nullptr)
        , Node(nullptr)
        , bLoop(true)
        , PlayRate(1.0f)
        , BlendInTime(0.3f)
        , BlendOutTime(0.3f)
        , InternalTime(0.0f)
        , PreviousInternalTime(0.0f)
    {
    }

    UPROPERTY(LuaReadWrite, EditAnywhere, Category="State")
    FName StateName;

    // ========================================
    // State 표현 방식 (두 가지 중 하나 사용)
    // ========================================

    /**
     * @brief Legacy: 직접 UAnimSequence 사용 (하위 호환용)
     *
     * 사용 케이스:
     * - 단순 애니메이션 재생 (Idle, Jump 등)
     * - 기존 코드와의 호환성 유지
     *
     * 우선순위:
     * - Node가 nullptr이면 Animation 사용
     * - Node가 있으면 Animation 무시
     */
    UPROPERTY(LuaReadWrite, EditAnywhere, Category="Animation")
    UAnimSequence* Animation = nullptr;

    /**
     * @brief New: 노드 기반 State (BlendSpace, SequencePlayer 등)
     *
     * 사용 케이스:
     * - Locomotion: BlendSpace1D (Idle/Walk/Run 블렌딩)
     * - Attack Combo: BlendSpace1D (Light/Heavy 강도 조절)
     * - Single Animation: SequencePlayer (Jump, Land 등)
     *
     * 우선순위:
     * - Node가 있으면 Node->Evaluate() 사용
     * - Node가 nullptr이면 Animation Fallback
     *
     * 예시 (Lua):
     *   local locomotionBS = self:CreateNodeByName("UAnimNode_BlendSpace1D")
     *   locomotionBS:AddSample(0, Idle)
     *   locomotionBS:AddSample(5, Walk)
     *   stateMachine:AddStateWithNode("Locomotion", locomotionBS)
     */
    UPROPERTY(LuaReadWrite, EditAnywhere, Category="Animation")
    UAnimNode* Node = nullptr;

    UPROPERTY(LuaReadWrite, EditAnywhere, Category="Playback")
    bool bLoop = true;

    UPROPERTY(LuaReadWrite, EditAnywhere, Category="Playback")
    float PlayRate = 1.0f;

    UPROPERTY(LuaReadWrite, EditAnywhere, Category="Blending")
    float BlendInTime = 0.3f;

    UPROPERTY(LuaReadWrite, EditAnywhere, Category="Blending")
    float BlendOutTime = 0.3f;

    // ========================================
    // Node-Centric 시간 관리
    // ========================================

    /**
     * @brief 현재 재생 시간
     *
     * 주의:
     * - Animation 모드: 이 값을 직접 사용
     * - Node 모드: Node 내부의 InternalTime 사용 (이 값은 사용 안 함)
     */
    float InternalTime = 0.0f;

    /**
     * @brief 이전 프레임 시간 (Notify 범위 검사용)
     *
     * 주의:
     * - Animation 모드: 이 값을 직접 사용
     * - Node 모드: Node 내부의 PreviousTime 사용
     */
    float PreviousInternalTime = 0.0f;
};
