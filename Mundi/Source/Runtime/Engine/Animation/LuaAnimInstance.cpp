#include "pch.h"
#include "LuaAnimInstance.h"
#include "LuaManager.h"
#include "SkeletalMeshComponent.h"
#include <sol/state.hpp>

ULuaAnimInstance::ULuaAnimInstance()
{
}

ULuaAnimInstance::~ULuaAnimInstance()
{
	CleanupLuaResources();
}

void ULuaAnimInstance::LuaInitializeAnimation()
{
	// Lua 초기화 - Base는 이미 NativeInitializeAnimation 호출 완료

	if (LuaScriptPath.empty())
	{
		UE_LOG("ULuaAnimInstance::LuaInitializeAnimation - LuaScriptPath is empty, skipping Lua initialization");
		return;
	}

	if (!OwnerComponent || !OwnerComponent->GetWorld())
	{
		UE_LOG("ULuaAnimInstance::LuaInitializeAnimation - OwnerComponent or World is null");
		return;
	}

	// ⭐ shared_ptr를 멤버로 저장하여 LuaManager 생명주기 보장
	LuaVM = OwnerComponent->GetWorld()->GetLuaManager();
	if (!LuaVM)
	{
		UE_LOG("ULuaAnimInstance::LuaInitializeAnimation - LuaManager is null");
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
		UE_LOG("[Lua][error] ULuaAnimInstance failed to load script: %s\n", LuaScriptPath.c_str());
#ifdef _EDITOR
		GEngine.EndPIE();
#endif
		CleanupLuaResources();
		return;
	}

	// 함수 캐시
	LuaInitFunc = FLuaManager::GetFunc(Env, "LuaInitializeAnimation");
	LuaUpdateFunc = FLuaManager::GetFunc(Env, "LuaUpdateAnimation");
	LuaGetPoseFunc = FLuaManager::GetFunc(Env, "LuaGetAnimationPose");

	// LuaInitializeAnimation 호출 - 여기서 Lua가 StateMachine 생성
	if (LuaInitFunc.valid())
	{
		auto Result = LuaInitFunc();
		if (!Result.valid())
		{
			sol::error Err = Result;
			UE_LOG("[Lua][error] ULuaAnimInstance::LuaInitializeAnimation failed: %s\n", Err.what());
#ifdef _EDITOR
			GEngine.EndPIE();
#endif
			return;
		}
	}
	else
	{
		UE_LOG("[Lua][warning] ULuaAnimInstance - LuaInitializeAnimation function not found in script: %s", LuaScriptPath.c_str());
	}

	bIsLuaInitialized = true;
	bIsLuaCleanedUp = false;

	UE_LOG("ULuaAnimInstance::LuaInitializeAnimation - Successfully initialized with script: %s", LuaScriptPath.c_str());
}

void ULuaAnimInstance::LuaUpdateAnimation(float DeltaSeconds)
{
	// Lua 업데이트 - Base는 이미 NativeUpdateAnimation 호출 완료

	if (!bIsLuaInitialized || bIsLuaCleanedUp)
		return;

	// Lua 함수 호출 - 상태 전환 로직 + StateMachine->Update 호출
	if (LuaUpdateFunc.valid())
	{
		auto Result = LuaUpdateFunc(DeltaSeconds);
		if (!Result.valid())
		{
			sol::error Err = Result;
			UE_LOG("[Lua][error] ULuaAnimInstance::LuaUpdateAnimation failed: %s\n", Err.what());
#ifdef _EDITOR
			GEngine.EndPIE();
#endif
		}
	}
}

void ULuaAnimInstance::GetAnimationPose(FPoseContext& OutPose)
{
	// Lua에서 StateMachine을 만들었다면 Lua 콜백 사용
	if (bIsLuaInitialized && !bIsLuaCleanedUp && LuaGetPoseFunc.valid())
	{
		// FPoseContext를 Lua에 포인터로 전달
		auto Result = LuaGetPoseFunc(&OutPose);
		if (!Result.valid())
		{
			sol::error Err = Result;
			UE_LOG("[Lua][error] ULuaAnimInstance::LuaGetAnimationPose failed: %s\n", Err.what());
#ifdef _EDITOR
			GEngine.EndPIE();
#endif
			// 에러 발생 시 기본 포즈 사용 (빈 포즈)
			Super::GetAnimationPose(OutPose);
		}
		return;
	}

	// Lua 함수가 없거나 초기화 실패 시 빈 포즈 반환
	Super::GetAnimationPose(OutPose);
}

void ULuaAnimInstance::CleanupLuaResources()
{
	if (bIsLuaCleanedUp)
		return;

	// ⭐ shared_ptr 패턴: LuaVM이 살아있으면 lua_State*도 유효 보장
	// 안전하게 명시적으로 해제
	if (LuaVM)
	{
		LuaInitFunc = sol::protected_function{};
		LuaUpdateFunc = sol::protected_function{};
		LuaGetPoseFunc = sol::protected_function{};
		Env = sol::environment{};
	}

	Lua = nullptr;
	LuaVM.reset();  // shared_ptr 명시적 해제
	bIsLuaInitialized = false;
	bIsLuaCleanedUp = true;

	UE_LOG("ULuaAnimInstance::CleanupLuaResources - Lua resources cleaned up");
}
