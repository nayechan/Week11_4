#pragma once

#include "AnimationStateMachine.h"
#include "ULuaAnimStateMachine.generated.h"

// ========================================
// Lua 애니메이션 스테이트 머신
// ========================================
//
// 사용 패턴 (String-based TMap + Lua metatable):
//
// 1. AnimInstance가 Variables (C++) + Vars wrapper (Lua) 소유:
//    function LuaInitializeAnimation()
//        -- self.Variables는 C++ TMap<FString, FString> (UPROPERTY)
//        -- self.Vars는 Lua metatable (자동 타입 변환)
//        self.Vars = setmetatable({}, {
//            __index = function(t, k)
//                local str = self.Variables[k]
//                return tonumber(str) or (str == "true") or str
//            end,
//            __newindex = function(t, k, v)
//                self.Variables[k] = tostring(v)
//            end
//        })
//        self.Vars["Speed"] = 0.0  -- Auto converted to "0.0"
//        self.Vars["bIsInAir"] = false  -- Auto converted to "false"
//    end
//
// 2. AnimInstance가 Vars 업데이트 (CharacterAnim.lua):
//    function LuaUpdateAnimation(deltaTime)
//        self.Vars["Speed"] = CalculateSpeed()  -- 자동 string 변환
//        self.StateMachine:Update(deltaTime)
//    end
//
// 3. StateMachine이 GetOuter()로 Variables 읽기 (CharacterSM.lua):
//    require(GDataDir .. "/Scripts/LuaUtils")
//    function LuaProcessState()
//        local anim = self:GetOuter()
//        local vars = CreateVarsWrapper(anim.Variables)  -- 각 environment에서 생성
//        if vars["Speed"] >= 300.0 then  -- 자동 number 변환
//            self:TransitionTo("Run")
//        end
//    end
//
// 책임 분리:
// - AnimInstance: 데이터 수집 및 소유 (Variables TMap)
// - StateMachine: 데이터 읽기 및 전환 결정
//
// 장점:
// - C++ TMap으로 저장 보장
// - Lua metatable로 타입 자동 변환 (편의성)
// - 런타임에 변수 자유롭게 추가/제거
//
// 타협안:
// - Blueprint는 정적 타입 → 명시적 UPROPERTY
// - Lua는 동적 타입 → TMap<FString, FString> + metatable
//
// ========================================

UCLASS(DisplayName="Lua 애니메이션 스테이트 머신", Description="Lua 스크립트로 제어되는 애니메이션 스테이트 머신입니다")
class ULuaAnimStateMachine : public UAnimStateMachine
{
public:
	GENERATED_REFLECTION_BODY()

	ULuaAnimStateMachine();
	~ULuaAnimStateMachine() override;

public:
	// ========================================
	// Virtual 함수 오버라이드
	// ========================================

	void LuaSetup() override;
	void ProcessState() override;

	void CleanupLuaResources();

protected:
	// Lua 스크립트 파일 경로 (에디터에서 설정 또는 Lua에서 동적 설정)
	UPROPERTY(LuaReadWrite, EditAnywhere, Category="Animation|Lua", Tooltip="Lua 스크립트 파일 경로입니다")
	FString LuaScriptPath{};

	// ⭐ ULuaScriptComponent 패턴: sol2가 pch.h에 있어서 값 멤버 사용 가능
	// ⭐ shared_ptr로 LuaManager 생명주기 보장 (dangling pointer 방지)
	std::shared_ptr<class FLuaManager> LuaVM;  // LuaManager 소유권 공유
	sol::state* Lua = nullptr;                  // 편의용 raw pointer (LuaVM->GetState())
	sol::environment Env{};

	// 함수 캐시
	sol::protected_function LuaSetupFunc{};
	sol::protected_function LuaProcessStateFunc{};

	bool bIsLuaInitialized = false;
	bool bIsLuaCleanedUp = false;

	friend class UAnimStateMachine;
};
