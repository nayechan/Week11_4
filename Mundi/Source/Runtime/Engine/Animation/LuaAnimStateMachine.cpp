#include "pch.h"
#include "LuaAnimStateMachine.h"
#include "AnimInstance.h"
#include "LuaManager.h"
#include <sol/state.hpp>

ULuaAnimStateMachine::ULuaAnimStateMachine()
{
}

ULuaAnimStateMachine::~ULuaAnimStateMachine()
{
	CleanupLuaResources();
}

void ULuaAnimStateMachine::LuaSetup()
{
	// Lua 초기화 - Base는 이미 NativeSetup 호출 완료
	UE_LOG("ULuaAnimStateMachine::LuaSetup - Called! LuaScriptPath = '%s'", LuaScriptPath.c_str());

	if (LuaScriptPath.empty())
	{
		UE_LOG("ULuaAnimStateMachine::LuaSetup - LuaScriptPath is empty, skipping Lua initialization");
		return;
	}

	// Outer 체인을 통해 World 가져오기
	UAnimInstance* AnimInst = Cast<UAnimInstance>(GetOuter());
	if (!AnimInst)
	{
		UE_LOG("ULuaAnimStateMachine::LuaSetup - Outer is not UAnimInstance");
		return;
	}

	UWorld* World = AnimInst->GetWorld();
	if (!World)
	{
		UE_LOG("ULuaAnimStateMachine::LuaSetup - World is null");
		return;
	}

	// ⭐ shared_ptr를 멤버로 저장하여 LuaManager 생명주기 보장
	LuaVM = World->GetLuaManager();
	if (!LuaVM)
	{
		UE_LOG("ULuaAnimStateMachine::LuaSetup - LuaManager is null");
		return;
	}

	Lua = &(LuaVM->GetState());  // 편의용 raw pointer

	// 독립된 환경 생성
	Env = LuaVM->CreateEnvironment();

	// self 바인딩 - LuaObjectProxy를 통해 리플렉션 기반 접근 제공
	Env["self"] = MakeCompProxy(*Lua, this, GetClass());

	// Lua 스크립트 로드
	if (!LuaVM->LoadScriptInto(Env, LuaScriptPath))
	{
		UE_LOG("[Lua][error] ULuaAnimStateMachine failed to load script: %s\n", LuaScriptPath.c_str());
#ifdef _EDITOR
		GEngine.EndPIE();
#endif
		CleanupLuaResources();
		return;
	}

	// 함수 캐시
	LuaSetupFunc = FLuaManager::GetFunc(Env, "LuaSetup");
	LuaProcessStateFunc = FLuaManager::GetFunc(Env, "LuaProcessState");

	// LuaSetup 호출 - 여기서 Lua가 States/Transitions 추가
	if (LuaSetupFunc.valid())
	{
		auto Result = LuaSetupFunc();
		if (!Result.valid())
		{
			sol::error Err = Result;
			UE_LOG("[Lua][error] ULuaAnimStateMachine::LuaSetup failed: %s\n", Err.what());
#ifdef _EDITOR
			GEngine.EndPIE();
#endif
			return;
		}
	}
	else
	{
		UE_LOG("[Lua][warning] ULuaAnimStateMachine - LuaSetup function not found in script: %s", LuaScriptPath.c_str());
	}

	bIsLuaInitialized = true;
	bIsLuaCleanedUp = false;

	UE_LOG("ULuaAnimStateMachine::LuaSetup - Successfully initialized with script: %s", LuaScriptPath.c_str());
}

void ULuaAnimStateMachine::ProcessState()
{
	// Lua 콜백 - 매 Update마다 호출 (선택적)

	if (!bIsLuaInitialized || bIsLuaCleanedUp)
		return;

	// Lua 함수 호출 - 상태별 커스텀 로직 (선택적)
	if (LuaProcessStateFunc.valid())
	{
		auto Result = LuaProcessStateFunc();
		if (!Result.valid())
		{
			sol::error Err = Result;
			UE_LOG("[Lua][error] ULuaAnimStateMachine::ProcessState failed: %s\n", Err.what());
#ifdef _EDITOR
			GEngine.EndPIE();
#endif
		}
	}
}

void ULuaAnimStateMachine::CleanupLuaResources()
{
	if (bIsLuaCleanedUp)
		return;

	// ⭐ shared_ptr 패턴: LuaVM이 살아있으면 lua_State*도 유효 보장
	// 안전하게 명시적으로 해제
	if (LuaVM)
	{
		LuaSetupFunc = sol::protected_function{};
		LuaProcessStateFunc = sol::protected_function{};
		Env = sol::environment{};
	}

	Lua = nullptr;
	LuaVM.reset();  // shared_ptr 명시적 해제
	bIsLuaInitialized = false;
	bIsLuaCleanedUp = true;

	UE_LOG("ULuaAnimStateMachine::CleanupLuaResources - Lua resources cleaned up");
}
