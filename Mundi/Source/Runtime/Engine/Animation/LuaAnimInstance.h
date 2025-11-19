#pragma once

#include "AnimGraphInstance.h"
#include "ULuaAnimInstance.generated.h"

/**
 * @brief Lua 스크립트로 노드 그래프를 동적 생성하는 AnimGraphInstance
 *
 * 상속 계층 변경:
 *   Before: ULuaAnimInstance : public UAnimInstance
 *   After:  ULuaAnimInstance : public UAnimGraphInstance ⭐
 *
 * 변경 이유:
 * - CreateNodeByName() 재사용 (Lua에서 동적 노드 생성)
 * - Nodes 배열 자동 관리 (GC, 초기화)
 * - RootNode 기반 평가 (노드 그래프 지원)
 *
 * 재사용 기능 (UAnimGraphInstance로부터):
 * - CreateNodeByName(NodeTypeName): Lua에서 노드 생성
 * - Nodes: GC 자동 관리, 노드 초기화
 * - RootNode: 트리 평가 진입점
 * - NativeUpdateAnimation(): RootNode->Update() 자동 호출
 *
 * Lua 사용 예시 (노드 그래프):
 *   function LuaInitializeAnimation()
 *       -- 1. StateMachine Node 생성
 *       local smNode = self:CreateNode("UAnimNode_StateMachine")
 *
 *       -- 2. StateMachine 설정
 *       local sm = NewObject("UAnimStateMachine", self:Get())
 *       sm:AddState(FName("Idle"), idleAnim, true, 1.0)
 *       sm:AddState(FName("Walk"), walkAnim, true, 1.0)
 *       sm:AddTransition(FName("Idle"), FName("Walk"), 0.5)
 *       sm:SetInitialState(FName("Idle"))
 *
 *       -- 3. Node에 StateMachine 연결
 *       smNode.StateMachine = sm
 *
 *       -- 4. RootNode 설정
 *       self.RootNode = smNode
 *   end
 *
 *   function LuaUpdateAnimation(deltaTime)
 *       -- 게임 로직: 변수 업데이트, 전환 조건 체크
 *       local speed = CalculateSpeed()
 *       if speed > 100 then
 *           smNode.StateMachine:TransitionTo(FName("Walk"))
 *       end
 *   end
 */
UCLASS(DisplayName="Lua 애니메이션 인스턴스", Description="Lua 스크립트로 노드 그래프를 동적 생성")
class ULuaAnimInstance : public UAnimGraphInstance
{
public:
	GENERATED_REFLECTION_BODY()

	ULuaAnimInstance();
	~ULuaAnimInstance() override;

public:
	void LuaInitializeAnimation() override;
	void LuaUpdateAnimation(float DeltaSeconds) override;
	void GetAnimationPose(FPoseContext& OutPose) override;

	void CleanupLuaResources();

	// Lua 스크립트 파일 경로 (에디터에서 설정 또는 런타임에서 동적 설정)
	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Animation|Lua", Tooltip="Lua 스크립트 파일 경로입니다")
	FString LuaScriptPath{};

	// ========================================
	// Gameplay Variables Dictionary
	// ========================================
	// String-based dictionary for Lua flexibility
	// Lua에서 파싱: tonumber(Variables["Speed"]), Variables["Name"] 등
	UPROPERTY(LuaReadWrite, Category="Animation|Lua", Tooltip="게임플레이 변수 딕셔너리 (String 기반)")
	TMap<FString, FString> Variables{};

protected:

	// ⭐ ULuaScriptComponent 패턴: sol2가 pch.h에 있어서 값 멤버 사용 가능
	// ⭐ shared_ptr로 LuaManager 생명주기 보장 (dangling pointer 방지)
	std::shared_ptr<class FLuaManager> LuaVM;  // LuaManager 소유권 공유
	sol::state* Lua = nullptr;                  // 편의용 raw pointer (LuaVM->GetState())
	sol::environment Env{};

	// 함수 캐시
	sol::protected_function LuaInitFunc{};
	sol::protected_function LuaUpdateFunc{};
	sol::protected_function LuaGetPoseFunc{};

	bool bIsLuaInitialized = false;
	bool bIsLuaCleanedUp = false;
};
